/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2022 tildearrow and contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _STRUCT_UPDATE_H
#define _STRUCT_UPDATE_H

#include "ta-log.h"
#include <refl.hpp>
#include <type_traits>
#include <memory>

#ifdef HAVE_NETWORKING
#include <msgpack.hpp>
#endif

namespace StructUpdate {
  // Forward declarations
  template<typename T>
  class Partial;

  struct Nested : refl::attr::usage::member {};

  namespace Internal {
    /**
     * @brief Used with `refl::trait::map_t` to provide storage type for the member
     */
    template<typename Member>
    struct MakeOptionalPartialStorage {
      using underlying_type = decltype(Member{}(std::declval<const typename Member::declaring_type&>()));

      /**
       * Remove const/volatile/reference qualifiers
       */
      using underlying_type_no_qual = refl::trait::remove_qualifiers_t<underlying_type>;

      /**
       * Remove const/volatile/reference qualifiers, and if the type is an array, remove the array's extent (So T[4] ->
       * T)
       */
      using underlying_type_no_qual_extent = std::remove_extent_t<underlying_type_no_qual>;

      static constexpr bool is_nested = refl::descriptor::has_attribute<Nested>(Member{});
      static constexpr bool is_array = std::is_array_v<refl::trait::remove_qualifiers_t<underlying_type>>;

      using type = std::conditional_t<
        is_nested,

        std::conditional_t<
          is_array,

          // Array of nested types get wrapped in std::array<std::shared_ptr<Type>, N>
          // Not wrapped in std::optional since we can treat nullptr as no-value
          std::array<std::shared_ptr<Partial<underlying_type_no_qual_extent>>, std::extent_v<underlying_type_no_qual>>,

          // Nested types get wrapped in std::shared_ptr<Type>
          // Not wrapped in std::optional since we can treat nullptr as no-value
          std::shared_ptr<Partial<underlying_type_no_qual_extent>>
        >,

        std::conditional_t<
          is_array,

          // Array of non-nested gets wrapped in std::array<std::optional<Type>, N>
          std::array<std::optional<underlying_type_no_qual_extent>, std::extent_v<underlying_type_no_qual>>,

          // Non-nested type get wrapped in std::optional<Type>
          std::optional<underlying_type_no_qual_extent>
        >
      >;
    };
  }

  /**
   * @brief A proxy which stores properties of the target type as std::optionals
   */
  template<typename T>
  class Partial : public refl::runtime::proxy<Partial<T>, T> {
    public:
      // Fields and property getters
      static constexpr auto members = filter(refl::member_list<T>{}, [](auto member) { return is_readable(member) && has_writer(member); });

      using MemberList = std::remove_cv_t<decltype(members)>;

      // Trap getter calls
      template<typename Member, typename Self, typename... Args>
      static decltype(auto) invoke_impl(Self&& self) {
        static_assert(is_readable(Member{}));
        return self.template get<Member>();
      }

      // Trap setter calls
      template<typename Member, typename Self, typename Value>
      static void invoke_impl(Self&& self, Value&& value) {
        static_assert(is_writable(Member{}));
        using GetterT = decltype(get_reader(Member{}));
        self.template get<GetterT>() = std::forward<Value>(value);
      }

      template<typename Member>
      auto& get() {
        constexpr size_t idx = refl::trait::index_of_v<Member, MemberList>;
        static_assert(idx != -1);
        return refl::util::get<idx>(data);
      }

      template<typename Member>
      const auto& get() const {
        constexpr size_t idx = refl::trait::index_of_v<Member, MemberList>;
        static_assert(idx != -1);
        return refl::util::get<idx>(data);
      }

#ifdef HAVE_NETWORKING
      // MsgPack serialization/deserialization methods, so a `Partial<T>` can be sent over the network

      /**
       * @brief Get the number of non-`std::nullopt` or non-null-`std::shared_ptr` fields in `this`
       */
      uint32_t getNumNonNulloptFields() const {
        uint32_t numNulloptFields = 0;
        for_each(members, [&](auto member) {
          using MemberType = decltype(member);

          auto optValue = get<MemberType>();

          if constexpr (Internal::MakeOptionalPartialStorage<MemberType>::is_array) {
            // Only count arrays if there's actually anything inside the array
            if (getNumNonNulloptFieldsInArray(optValue) > 0) numNulloptFields++;
          } else {
            if (optValue) numNulloptFields++;
          }
        });

        return numNulloptFields;
      }

      /**
       * @brief Get the number of non-`std::nullopt` or non-null-`std::shared_ptr` fields in the given array
       *
       * @tparam U Either `std::optional` or `std::shared_ptr`
       * @tparam N Length of the array
       */
      template<typename U, size_t N>
      static uint32_t getNumNonNulloptFieldsInArray(const std::array<U, N>& array) {
        uint32_t numNulloptFields = 0;
        for (const U& item : array) {
          if (item) numNulloptFields++;
        }

        return numNulloptFields;
      }

      template<typename Packer>
      void msgpack_pack(Packer& msgpack_pk) const {
        // Pack all the non-nullopt members into a msgpack map
        msgpack_pk.pack_map(getNumNonNulloptFields());

        for_each(members, [&](auto member) {
          using MemberType = decltype(member);

          auto optValue = get<MemberType>();

          if constexpr (Internal::MakeOptionalPartialStorage<MemberType>::is_array) {
            // It's a std::array<std::optional<Type>> or std::array<std::shared_ptr<Type>>
            // Serialize this into a map of keys (array indices) and values, since we only want to serialize non-null
            // array values
            uint32_t arrayNumToPack = getNumNonNulloptFieldsInArray(optValue);
            if (arrayNumToPack > 0) {
              msgpack_pk.pack_str(member.name.size);
              msgpack_pk.pack_str_body(member.name.c_str());

              msgpack_pk.pack_array(arrayNumToPack);

              uint32_t i = 0;
              for (const auto& arrayValue : optValue) {
                if (arrayValue) {
                  // This array value is non-null, pack it
                  msgpack_pk.pack_uint32(i);
                  msgpack_pk.pack(*arrayValue);
                }

                i++;
              }
            }
          } else {
            // It's a std::optional<Type> or std::shared_ptr<Type>
            if (optValue) {
              msgpack_pk.pack_str(member.name.size);
              msgpack_pk.pack_str_body(member.name.c_str());

              msgpack_pk.pack(*optValue);
            }
          }
        });
      }

      /**
       * @throw msgpack::type_error on failure
       */
      void msgpack_unpack(const msgpack::object& o) {
        if (o.type != msgpack::type::MAP) {
          logE("Tried to deserialize into a Partial<T>, but the object wasn't a map\n");
          throw msgpack::type_error();
        }

        for (uint32_t i = 0; i < o.via.map.size; i++) {
          const msgpack::object_kv& kv = o.via.map.ptr[i];

          // Get the field name from the map key
          if (kv.key.type != msgpack::type::STR) {
            logE("Tried to deserialize into a Partial<T>, but a map key wasn't a string\n");
            throw msgpack::type_error();
          }
          std::string fieldName;
          fieldName.assign(kv.key.via.str.ptr, kv.key.via.str.size);

          // Try to find the field, and try set its value
          bool found = false;
          for_each(members, [&](auto member) {
            using MemberType = decltype(member);

            if (found) return;

            if (std::strcmp(member.name.c_str(), fieldName.c_str()) == 0) {
              if constexpr (Internal::MakeOptionalPartialStorage<MemberType>::is_nested) {
                if constexpr (Internal::MakeOptionalPartialStorage<MemberType>::is_array) {
                  // It's a std::array<std::shared_ptr<Type>>
                  if (kv.val.type != msgpack::type::MAP) {
                    logE("Tried to deserialize an array in Partial<T>, but the serialized array type wasn't a map\n");
                    throw msgpack::type_error();
                  }
                  for (uint32_t j = 0; j < kv.val.via.map.size; j++) {
                    // The key should be the array index
                    if (kv.val.via.map.ptr[j].key.type != msgpack::type::POSITIVE_INTEGER) {
                      logE("Tried to deserialize an array in Partial<T>, but the serialized array type wasn't a positive integer (it was type #%u)\n", kv.key.type);
                      throw msgpack::type_error();
                    }
                    uint64_t arrayIndex = kv.val.via.map.ptr[j].key.via.u64;

                    // Check if it's out-of-bounds for the array
                    if (arrayIndex >= std::extent_v<typename MemberType::value_type>) {
                      logE("Tried to deserialize an array in Partial<T>, an array index was out-of-bounds\n");
                      throw msgpack::type_error();
                    }

                    // Try deserialize it into a value
                    Partial<typename std::remove_extent_t<typename MemberType::value_type>> newVal;
                    try {
                      kv.val.via.map.ptr[j].val.convert(newVal);
                    } catch (msgpack::type_error& e) {
                      logE("Tried to deserialize an array value in Partial<T>, but deserializing it failed\n");
                      throw;
                    }

                    auto optValue = &get<MemberType>();
                    (*optValue)[arrayIndex] = std::make_shared<Partial<typename std::remove_extent_t<typename MemberType::value_type>>>(std::move(newVal));
                  }
                } else {
                  // It's a std::shared_ptr<Type>
                  Partial<typename MemberType::value_type> newVal;
                  kv.val.convert(newVal);

                  auto optValue = &get<MemberType>();
                  *optValue = std::make_shared<Partial<typename MemberType::value_type>>(std::move(newVal));
                }
              } else {
                if constexpr (Internal::MakeOptionalPartialStorage<MemberType>::is_array) {
                  // It's a std::array<std::optional<Type>>
                  if (kv.val.type != msgpack::type::MAP) {
                    logE("Tried to deserialize an array in Partial<T>, but the serialized array type wasn't a map\n");
                    throw msgpack::type_error();
                  }
                  for (uint32_t j = 0; j < kv.val.via.map.size; j++) {
                    // The key should be the array index
                    if (kv.val.via.map.ptr[j].key.type != msgpack::type::POSITIVE_INTEGER) {
                      logE("Tried to deserialize an array in Partial<T>, but the serialized array type wasn't a positive integer (it was type #%u)\n", kv.key.type);
                      throw msgpack::type_error();
                    }
                    uint64_t arrayIndex = kv.val.via.map.ptr[j].key.via.u64;

                    // Check if it's out-of-bounds for the array
                    if (arrayIndex >= std::extent_v<typename MemberType::value_type>) {
                      logE("Tried to deserialize an array in Partial<T>, an array index was out-of-bounds\n");
                      throw msgpack::type_error();
                    }

                    // Try deserialize it into a value
                    typename std::remove_extent_t<typename MemberType::value_type> newVal;
                    try {
                      kv.val.via.map.ptr[j].val.convert(newVal);
                    } catch (msgpack::type_error& e) {
                      logE("Tried to deserialize an array value in Partial<T>, but deserializing it failed\n");
                      throw;
                    }

                    auto optValue = &get<MemberType>();
                    (*optValue)[arrayIndex].emplace(std::move(newVal));
                  }
                } else {
                  // It's a std::optional<Type>
                  typename MemberType::value_type newVal;
                  kv.val.convert(newVal);

                  auto optValue = &get<MemberType>();
                  optValue->emplace(std::move(newVal));
                }
              }
            }
          });
        }
      }

      template<typename MSGPACK_OBJECT>
      void msgpack_object(MSGPACK_OBJECT* o, msgpack::zone& z) const {
        uint32_t numToPack = getNumNonNulloptFields();

        o->type = msgpack::type::MAP;
        o->via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv) * numToPack, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
        o->via.map.size = numToPack;

        uint32_t i = 0;
        for_each(members, [&](auto member) {
          using MemberType = decltype(member);

          auto optValue = get<MemberType>();
          if constexpr (Internal::MakeOptionalPartialStorage<MemberType>::is_array) {
            // It's a std::array<std::optional<Type>> or std::array<std::shared_ptr<Type>>
            // Serialize this into a map of keys (array indices) and values, since we only want to serialize non-null
            // array values
            uint32_t arrayNumToPack = getNumNonNulloptFieldsInArray(optValue);
            if (arrayNumToPack > 0) {
              o->via.map.ptr[i].key = msgpack::object(member.name.c_str(), z);

              msgpack::object arrayObj;
              arrayObj.type = msgpack::type::MAP;
              arrayObj.via.map.ptr = static_cast<msgpack::object_kv*>(z.allocate_align(sizeof(msgpack::object_kv) * arrayNumToPack, MSGPACK_ZONE_ALIGNOF(msgpack::object_kv)));
              arrayObj.via.map.size = numToPack;

              uint32_t j = 0;
              uint32_t k = 0;
              for (const auto& arrayValue : optValue) {
                if (arrayValue) {
                  // This array value is non-null, pack it
                  arrayObj.via.map.ptr[k].key = msgpack::object(j, z);
                  arrayObj.via.map.ptr[k].val = msgpack::object(*arrayValue, z);

                  k++;
                }

                j++;
              }

              o->via.map.ptr[i].key = msgpack::object(member.name.c_str(), z);
              o->via.map.ptr[i].val = arrayObj;

              i++;
            }
          } else {
            // It's a std::optional or std::shared_ptr
            if (optValue) {
              o->via.map.ptr[i].key = msgpack::object(member.name.c_str(), z);
              o->via.map.ptr[i].val = msgpack::object(*optValue, z);

              i++;
            }
          }
        });
      }
#endif

    private:
      using MemberStorageList = refl::trait::map_t<Internal::MakeOptionalPartialStorage, MemberList>;

      refl::trait::as_tuple_t<MemberStorageList> data;
  };

  template<typename T>
  void update(T& target, const Partial<T>& source) {
    for_each(source.members, [&](auto member) {
      using MemberType = decltype(member);

      auto optValue = source.template get<MemberType>();

      // If MemberType is nested, drill down into updating sub-fields
      if constexpr (Internal::MakeOptionalPartialStorage<MemberType>::is_nested) {
        if constexpr (Internal::MakeOptionalPartialStorage<MemberType>::is_array) {
          // It's a std::array<std::shared_ptr<Type>>
          size_t i = 0;
          for (const auto& item : optValue) {
            if (item) {
              constexpr auto writer = get_writer(member);
              update<std::remove_extent_t<typename MemberType::value_type>>(writer(target)[i], *item);
            }

            i++;
          }
        } else {
          // It's a std::shared_ptr<Type>
          if (optValue) {
            constexpr auto writer = get_writer(member);
            update<typename MemberType::value_type>(writer(target), *optValue);
          }
        }
      } else {
        if constexpr (Internal::MakeOptionalPartialStorage<MemberType>::is_array) {
          // It's a std::array<std::optional<Type>>
          size_t i = 0;
          for (const auto& item : optValue) {
            if (item) {
              constexpr auto writer = get_writer(member);
              writer(target)[i] = *item;
            }

            i++;
          }
        } else {
          // It's a std::optional<Type>
          if (optValue) {
            constexpr auto writer = get_writer(member);
            writer(target, *optValue);
          }
        }
      }
    });
  }
}

#endif
