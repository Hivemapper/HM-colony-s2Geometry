// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#ifndef S2_VALUE_LEXICON_H_
#define S2_VALUE_LEXICON_H_

#include <functional>
#include <limits>
#include <vector>

#include "s2/third_party/absl/base/integral_types.h"
#include "s2/util/gtl/dense_hash_set.h"

// ValueLexicon is a class that maps distinct values to sequentially numbered
// integer identifiers.  It automatically eliminates duplicates and uses a
// compact representation.  See also SequenceLexicon.
//
// Each distinct value is mapped to a 32-bit integer.  The space used for each
// value is approximately 7 bytes plus the space needed for the value itself.
// For example, int64 values would need approximately 15 bytes each.  Note
// also that values are referred to using 32-bit ids rather than 64-bit
// pointers.
//
// This class has the same thread-safety properties as "string": const methods
// are thread safe, and non-const methods are not thread safe.
//
// Example usage:
//
//   ValueLexicon<string> lexicon;
//   uint32 cat_id = lexicon.Add("cat");
//   EXPECT_EQ(cat_id, lexicon.Add("cat"));
//   EXPECT_EQ("cat", lexicon.value(cat_id));
//
template <class T,
          class Hasher = std::hash<T>,
          class KeyEqual = std::equal_to<T>>
class ValueLexicon {
 public:
  explicit ValueLexicon(Hasher const& hasher = Hasher(),
                        KeyEqual const& key_equal = KeyEqual());

  // Clears all data from the lexicon.
  void Clear();

  // Add the given value to the lexicon if it is not already present, and
  // return its integer id.  Ids are assigned sequentially starting from zero.
  uint32 Add(T const& value);

  // Return the number of values in the lexicon.
  uint32 size() const;

  // Return the value with the given id.
  T const& value(uint32 id) const;

 private:
  friend class IdKeyEqual;
  // Choose kEmptyKey to be the last key that will ever be generated.
  static uint32 const kEmptyKey = std::numeric_limits<uint32>::max();

  class IdHasher {
   public:
    IdHasher(Hasher const& hasher, ValueLexicon const& lexicon);
    size_t operator()(uint32 id) const;
   private:
    Hasher hasher_;
    ValueLexicon const& lexicon_;
  };

  class IdKeyEqual {
   public:
    IdKeyEqual(KeyEqual const& key_equal, ValueLexicon const& lexicon);
    bool operator()(uint32 id1, uint32 id2) const;
   private:
    KeyEqual key_equal_;
    ValueLexicon const& lexicon_;
  };

  using IdSet =
      google::dense_hash_set<uint32, IdHasher, IdKeyEqual>;

  KeyEqual key_equal_;
  std::vector<T> values_;
  IdSet id_set_;

  ValueLexicon(ValueLexicon const&) = delete;
  ValueLexicon& operator=(ValueLexicon const&) = delete;
};


//////////////////   Implementation details follow   ////////////////////


template <class T, class Hasher, class KeyEqual>
uint32 const ValueLexicon<T, Hasher, KeyEqual>::kEmptyKey;

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>::IdHasher::IdHasher(
    Hasher const& hasher, ValueLexicon const& lexicon)
    : hasher_(hasher), lexicon_(lexicon) {
}

template <class T, class Hasher, class KeyEqual>
inline size_t ValueLexicon<T, Hasher, KeyEqual>::IdHasher::operator()(
    uint32 id) const {
  return hasher_(lexicon_.value(id));
}

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>::IdKeyEqual::IdKeyEqual(
    KeyEqual const& key_equal, ValueLexicon const& lexicon)
    : key_equal_(key_equal), lexicon_(lexicon) {
}

template <class T, class Hasher, class KeyEqual>
inline bool ValueLexicon<T, Hasher, KeyEqual>::IdKeyEqual::operator()(
    uint32 id1, uint32 id2) const {
  if (id1 == id2) return true;
  if (id1 == lexicon_.kEmptyKey || id2 == lexicon_.kEmptyKey) {
    return false;
  }
  return key_equal_(lexicon_.value(id1), lexicon_.value(id2));
}

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>::ValueLexicon(Hasher const& hasher,
                                                KeyEqual const& key_equal)
    : key_equal_(key_equal),
      id_set_(0, IdHasher(hasher, *this),
                 IdKeyEqual(key_equal, *this)) {
  id_set_.set_empty_key(kEmptyKey);
}

template <class T, class Hasher, class KeyEqual>
void ValueLexicon<T, Hasher, KeyEqual>::Clear() {
  values_.clear();
  id_set_.clear();
}

template <class T, class Hasher, class KeyEqual>
uint32 ValueLexicon<T, Hasher, KeyEqual>::Add(T const& value) {
  if (!values_.empty() && key_equal_(value, values_.back())) {
    return values_.size() - 1;
  }
  values_.push_back(value);
  auto result = id_set_.insert(values_.size() - 1);
  if (result.second) {
    return values_.size() - 1;
  } else {
    values_.pop_back();
    return *result.first;
  }
}

template <class T, class Hasher, class KeyEqual>
inline uint32 ValueLexicon<T, Hasher, KeyEqual>::size() const {
  return values_.size();
}

template <class T, class Hasher, class KeyEqual>
inline T const& ValueLexicon<T, Hasher, KeyEqual>::value(uint32 id) const {
  return values_[id];
}

#endif  // S2_VALUE_LEXICON_H_