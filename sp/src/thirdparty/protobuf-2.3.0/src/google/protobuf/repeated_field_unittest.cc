// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// TODO(kenton):  Improve this unittest to bring it up to the standards of
//   other proto2 unittests.

#include <algorithm>
#include <list>
#include <vector>

#include <google/protobuf/repeated_field.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/stl_util-inl.h>

namespace google {
using protobuf_unittest::TestAllTypes;

namespace protobuf {
namespace {

// Test operations on a RepeatedField which is small enough that it does
// not allocate a separate array for storage.
TEST(RepeatedField, Small) {
  RepeatedField<int> field;

  EXPECT_EQ(field.size(), 0);

  field.Add(5);

  EXPECT_EQ(field.size(), 1);
  EXPECT_EQ(field.Get(0), 5);

  field.Add(42);

  EXPECT_EQ(field.size(), 2);
  EXPECT_EQ(field.Get(0), 5);
  EXPECT_EQ(field.Get(1), 42);

  field.Set(1, 23);

  EXPECT_EQ(field.size(), 2);
  EXPECT_EQ(field.Get(0), 5);
  EXPECT_EQ(field.Get(1), 23);
  EXPECT_EQ(field.SpaceUsedExcludingSelf(), 0);

  field.RemoveLast();

  EXPECT_EQ(field.size(), 1);
  EXPECT_EQ(field.Get(0), 5);

  field.Clear();

  EXPECT_EQ(field.size(), 0);
  EXPECT_EQ(field.SpaceUsedExcludingSelf(), 0);
}

// Test operations on a RepeatedField which is large enough to allocate a
// separate array.
TEST(RepeatedField, Large) {
  RepeatedField<int> field;

  for (int i = 0; i < 16; i++) {
    field.Add(i * i);
  }

  EXPECT_EQ(field.size(), 16);

  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(field.Get(i), i * i);
  }

  int expected_usage = 16 * sizeof(int);
  EXPECT_GE(field.SpaceUsedExcludingSelf(), expected_usage);
}

// Test swapping between various types of RepeatedFields.
TEST(RepeatedField, SwapSmallSmall) {
  RepeatedField<int> field1;
  RepeatedField<int> field2;

  field1.Add(5);
  field1.Add(42);

  field1.Swap(&field2);

  EXPECT_EQ(field1.size(), 0);
  EXPECT_EQ(field2.size(), 2);
  EXPECT_EQ(field2.Get(0), 5);
  EXPECT_EQ(field2.Get(1), 42);
}

TEST(RepeatedField, SwapLargeSmall) {
  RepeatedField<int> field1;
  RepeatedField<int> field2;

  for (int i = 0; i < 16; i++) {
    field1.Add(i * i);
  }
  field2.Add(5);
  field2.Add(42);
  field1.Swap(&field2);

  EXPECT_EQ(field1.size(), 2);
  EXPECT_EQ(field1.Get(0), 5);
  EXPECT_EQ(field1.Get(1), 42);
  EXPECT_EQ(field2.size(), 16);
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(field2.Get(i), i * i);
  }
}

TEST(RepeatedField, SwapLargeLarge) {
  RepeatedField<int> field1;
  RepeatedField<int> field2;

  field1.Add(5);
  field1.Add(42);
  for (int i = 0; i < 16; i++) {
    field1.Add(i);
    field2.Add(i * i);
  }
  field2.Swap(&field1);

  EXPECT_EQ(field1.size(), 16);
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(field1.Get(i), i * i);
  }
  EXPECT_EQ(field2.size(), 18);
  EXPECT_EQ(field2.Get(0), 5);
  EXPECT_EQ(field2.Get(1), 42);
  for (int i = 2; i < 18; i++) {
    EXPECT_EQ(field2.Get(i), i - 2);
  }
}

// Determines how much space was reserved by the given field by adding elements
// to it until it re-allocates its space.
static int ReservedSpace(RepeatedField<int>* field) {
  const int* ptr = field->data();
  do {
    field->Add(0);
  } while (field->data() == ptr);

  return field->size() - 1;
}

TEST(RepeatedField, ReserveMoreThanDouble) {
  // Reserve more than double the previous space in the field and expect the
  // field to reserve exactly the amount specified.
  RepeatedField<int> field;
  field.Reserve(20);

  EXPECT_EQ(20, ReservedSpace(&field));
}

TEST(RepeatedField, ReserveLessThanDouble) {
  // Reserve less than double the previous space in the field and expect the
  // field to grow by double instead.
  RepeatedField<int> field;
  field.Reserve(20);
  field.Reserve(30);

  EXPECT_EQ(40, ReservedSpace(&field));
}

TEST(RepeatedField, ReserveLessThanExisting) {
  // Reserve less than the previous space in the field and expect the
  // field to not re-allocate at all.
  RepeatedField<int> field;
  field.Reserve(20);
  const int* previous_ptr = field.data();
  field.Reserve(10);

  EXPECT_EQ(previous_ptr, field.data());
  EXPECT_EQ(20, ReservedSpace(&field));
}

TEST(RepeatedField, MergeFrom) {
  RepeatedField<int> source, destination;

  source.Add(4);
  source.Add(5);

  destination.Add(1);
  destination.Add(2);
  destination.Add(3);

  destination.MergeFrom(source);

  ASSERT_EQ(5, destination.size());

  EXPECT_EQ(1, destination.Get(0));
  EXPECT_EQ(2, destination.Get(1));
  EXPECT_EQ(3, destination.Get(2));
  EXPECT_EQ(4, destination.Get(3));
  EXPECT_EQ(5, destination.Get(4));
}

TEST(RepeatedField, MutableDataIsMutable) {
  RepeatedField<int> field;
  field.Add(1);
  EXPECT_EQ(1, field.Get(0));
  // The fact that this line compiles would be enough, but we'll check the
  // value anyway.
  *field.mutable_data() = 2;
  EXPECT_EQ(2, field.Get(0));
}

TEST(RepeatedField, Truncate) {
  RepeatedField<int> field;

  field.Add(12);
  field.Add(34);
  field.Add(56);
  field.Add(78);
  EXPECT_EQ(4, field.size());

  field.Truncate(3);
  EXPECT_EQ(3, field.size());

  field.Add(90);
  EXPECT_EQ(4, field.size());
  EXPECT_EQ(90, field.Get(3));

  // Truncations that don't change the size are allowed, but growing is not
  // allowed.
  field.Truncate(field.size());
#ifdef GTEST_HAS_DEATH_TEST
  EXPECT_DEBUG_DEATH(field.Truncate(field.size() + 1), "new_size");
#endif
}


// ===================================================================
// RepeatedPtrField tests.  These pretty much just mirror the RepeatedField
// tests above.

TEST(RepeatedPtrField, Small) {
  RepeatedPtrField<string> field;

  EXPECT_EQ(field.size(), 0);

  field.Add()->assign("foo");

  EXPECT_EQ(field.size(), 1);
  EXPECT_EQ(field.Get(0), "foo");

  field.Add()->assign("bar");

  EXPECT_EQ(field.size(), 2);
  EXPECT_EQ(field.Get(0), "foo");
  EXPECT_EQ(field.Get(1), "bar");

  field.Mutable(1)->assign("baz");

  EXPECT_EQ(field.size(), 2);
  EXPECT_EQ(field.Get(0), "foo");
  EXPECT_EQ(field.Get(1), "baz");

  field.RemoveLast();

  EXPECT_EQ(field.size(), 1);
  EXPECT_EQ(field.Get(0), "foo");

  field.Clear();

  EXPECT_EQ(field.size(), 0);
}

TEST(RepeatedPtrField, Large) {
  RepeatedPtrField<string> field;

  for (int i = 0; i < 16; i++) {
    *field.Add() += 'a' + i;
  }

  EXPECT_EQ(field.size(), 16);

  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(field.Get(i).size(), 1);
    EXPECT_EQ(field.Get(i)[0], 'a' + i);
  }

  int min_expected_usage = 16 * sizeof(string);
  EXPECT_GE(field.SpaceUsedExcludingSelf(), min_expected_usage);
}

TEST(RepeatedPtrField, SwapSmallSmall) {
  RepeatedPtrField<string> field1;
  RepeatedPtrField<string> field2;

  field1.Add()->assign("foo");
  field1.Add()->assign("bar");
  field1.Swap(&field2);

  EXPECT_EQ(field1.size(), 0);
  EXPECT_EQ(field2.size(), 2);
  EXPECT_EQ(field2.Get(0), "foo");
  EXPECT_EQ(field2.Get(1), "bar");
}

TEST(RepeatedPtrField, SwapLargeSmall) {
  RepeatedPtrField<string> field1;
  RepeatedPtrField<string> field2;

  field2.Add()->assign("foo");
  field2.Add()->assign("bar");
  for (int i = 0; i < 16; i++) {
    *field1.Add() += 'a' + i;
  }
  field1.Swap(&field2);

  EXPECT_EQ(field1.size(), 2);
  EXPECT_EQ(field1.Get(0), "foo");
  EXPECT_EQ(field1.Get(1), "bar");
  EXPECT_EQ(field2.size(), 16);
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(field2.Get(i).size(), 1);
    EXPECT_EQ(field2.Get(i)[0], 'a' + i);
  }
}

TEST(RepeatedPtrField, SwapLargeLarge) {
  RepeatedPtrField<string> field1;
  RepeatedPtrField<string> field2;

  field1.Add()->assign("foo");
  field1.Add()->assign("bar");
  for (int i = 0; i < 16; i++) {
    *field1.Add() += 'A' + i;
    *field2.Add() += 'a' + i;
  }
  field2.Swap(&field1);

  EXPECT_EQ(field1.size(), 16);
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(field1.Get(i).size(), 1);
    EXPECT_EQ(field1.Get(i)[0], 'a' + i);
  }
  EXPECT_EQ(field2.size(), 18);
  EXPECT_EQ(field2.Get(0), "foo");
  EXPECT_EQ(field2.Get(1), "bar");
  for (int i = 2; i < 18; i++) {
    EXPECT_EQ(field2.Get(i).size(), 1);
    EXPECT_EQ(field2.Get(i)[0], 'A' + i - 2);
  }
}

static int ReservedSpace(RepeatedPtrField<string>* field) {
  const string* const* ptr = field->data();
  do {
    field->Add();
  } while (field->data() == ptr);

  return field->size() - 1;
}

TEST(RepeatedPtrField, ReserveMoreThanDouble) {
  RepeatedPtrField<string> field;
  field.Reserve(20);

  EXPECT_EQ(20, ReservedSpace(&field));
}

TEST(RepeatedPtrField, ReserveLessThanDouble) {
  RepeatedPtrField<string> field;
  field.Reserve(20);
  field.Reserve(30);

  EXPECT_EQ(40, ReservedSpace(&field));
}

TEST(RepeatedPtrField, ReserveLessThanExisting) {
  RepeatedPtrField<string> field;
  field.Reserve(20);
  const string* const* previous_ptr = field.data();
  field.Reserve(10);

  EXPECT_EQ(previous_ptr, field.data());
  EXPECT_EQ(20, ReservedSpace(&field));
}

TEST(RepeatedPtrField, ReserveDoesntLoseAllocated) {
  // Check that a bug is fixed:  An earlier implementation of Reserve()
  // failed to copy pointers to allocated-but-cleared objects, possibly
  // leading to segfaults.
  RepeatedPtrField<string> field;
  string* first = field.Add();
  field.RemoveLast();

  field.Reserve(20);
  EXPECT_EQ(first, field.Add());
}

// Clearing elements is tricky with RepeatedPtrFields since the memory for
// the elements is retained and reused.
TEST(RepeatedPtrField, ClearedElements) {
  RepeatedPtrField<string> field;

  string* original = field.Add();
  *original = "foo";

  EXPECT_EQ(field.ClearedCount(), 0);

  field.RemoveLast();
  EXPECT_TRUE(original->empty());
  EXPECT_EQ(field.ClearedCount(), 1);

  EXPECT_EQ(field.Add(), original);  // Should return same string for reuse.

  EXPECT_EQ(field.ReleaseLast(), original);  // We take ownership.
  EXPECT_EQ(field.ClearedCount(), 0);

  EXPECT_NE(field.Add(), original);  // Should NOT return the same string.
  EXPECT_EQ(field.ClearedCount(), 0);

  field.AddAllocated(original);  // Give ownership back.
  EXPECT_EQ(field.ClearedCount(), 0);
  EXPECT_EQ(field.Mutable(1), original);

  field.Clear();
  EXPECT_EQ(field.ClearedCount(), 2);
  EXPECT_EQ(field.ReleaseCleared(), original);  // Take ownership again.
  EXPECT_EQ(field.ClearedCount(), 1);
  EXPECT_NE(field.Add(), original);
  EXPECT_EQ(field.ClearedCount(), 0);
  EXPECT_NE(field.Add(), original);
  EXPECT_EQ(field.ClearedCount(), 0);

  field.AddCleared(original);  // Give ownership back, but as a cleared object.
  EXPECT_EQ(field.ClearedCount(), 1);
  EXPECT_EQ(field.Add(), original);
  EXPECT_EQ(field.ClearedCount(), 0);
}

// Test all code paths in AddAllocated().
TEST(RepeatedPtrField, AddAlocated) {
  RepeatedPtrField<string> field;
  while (field.size() < field.Capacity()) {
    field.Add()->assign("filler");
  }

  int index = field.size();

  // First branch:  Field is at capacity with no cleared objects.
  string* foo = new string("foo");
  field.AddAllocated(foo);
  EXPECT_EQ(index + 1, field.size());
  EXPECT_EQ(0, field.ClearedCount());
  EXPECT_EQ(foo, &field.Get(index));

  // Last branch:  Field is not at capacity and there are no cleared objects.
  string* bar = new string("bar");
  field.AddAllocated(bar);
  ++index;
  EXPECT_EQ(index + 1, field.size());
  EXPECT_EQ(0, field.ClearedCount());
  EXPECT_EQ(bar, &field.Get(index));

  // Third branch:  Field is not at capacity and there are no cleared objects.
  field.RemoveLast();
  string* baz = new string("baz");
  field.AddAllocated(baz);
  EXPECT_EQ(index + 1, field.size());
  EXPECT_EQ(1, field.ClearedCount());
  EXPECT_EQ(baz, &field.Get(index));

  // Second branch:  Field is at capacity but has some cleared objects.
  while (field.size() < field.Capacity()) {
    field.Add()->assign("filler2");
  }
  field.RemoveLast();
  index = field.size();
  string* qux = new string("qux");
  field.AddAllocated(qux);
  EXPECT_EQ(index + 1, field.size());
  // We should have discarded the cleared object.
  EXPECT_EQ(0, field.ClearedCount());
  EXPECT_EQ(qux, &field.Get(index));
}

TEST(RepeatedPtrField, MergeFrom) {
  RepeatedPtrField<string> source, destination;

  source.Add()->assign("4");
  source.Add()->assign("5");

  destination.Add()->assign("1");
  destination.Add()->assign("2");
  destination.Add()->assign("3");

  destination.MergeFrom(source);

  ASSERT_EQ(5, destination.size());

  EXPECT_EQ("1", destination.Get(0));
  EXPECT_EQ("2", destination.Get(1));
  EXPECT_EQ("3", destination.Get(2));
  EXPECT_EQ("4", destination.Get(3));
  EXPECT_EQ("5", destination.Get(4));
}

TEST(RepeatedPtrField, MutableDataIsMutable) {
  RepeatedPtrField<string> field;
  *field.Add() = "1";
  EXPECT_EQ("1", field.Get(0));
  // The fact that this line compiles would be enough, but we'll check the
  // value anyway.
  string** data = field.mutable_data();
  **data = "2";
  EXPECT_EQ("2", field.Get(0));
}

// ===================================================================

// Iterator tests stolen from net/proto/proto-array_unittest.
class RepeatedFieldIteratorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    for (int i = 0; i < 3; ++i) {
      proto_array_.Add(i);
    }
  }

  RepeatedField<int> proto_array_;
};

TEST_F(RepeatedFieldIteratorTest, Convertible) {
  RepeatedField<int>::iterator iter = proto_array_.begin();
  RepeatedField<int>::const_iterator c_iter = iter;
  EXPECT_EQ(0, *c_iter);
}

TEST_F(RepeatedFieldIteratorTest, MutableIteration) {
  RepeatedField<int>::iterator iter = proto_array_.begin();
  EXPECT_EQ(0, *iter);
  ++iter;
  EXPECT_EQ(1, *iter++);
  EXPECT_EQ(2, *iter);
  ++iter;
  EXPECT_TRUE(proto_array_.end() == iter);

  EXPECT_EQ(2, *(proto_array_.end() - 1));
}

TEST_F(RepeatedFieldIteratorTest, ConstIteration) {
  const RepeatedField<int>& const_proto_array = proto_array_;
  RepeatedField<int>::const_iterator iter = const_proto_array.begin();
  EXPECT_EQ(0, *iter);
  ++iter;
  EXPECT_EQ(1, *iter++);
  EXPECT_EQ(2, *iter);
  ++iter;
  EXPECT_TRUE(proto_array_.end() == iter);
  EXPECT_EQ(2, *(proto_array_.end() - 1));
}

TEST_F(RepeatedFieldIteratorTest, Mutation) {
  RepeatedField<int>::iterator iter = proto_array_.begin();
  *iter = 7;
  EXPECT_EQ(7, proto_array_.Get(0));
}

// -------------------------------------------------------------------

class RepeatedPtrFieldIteratorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    proto_array_.Add()->assign("foo");
    proto_array_.Add()->assign("bar");
    proto_array_.Add()->assign("baz");
  }

  RepeatedPtrField<string> proto_array_;
};

TEST_F(RepeatedPtrFieldIteratorTest, Convertible) {
  RepeatedPtrField<string>::iterator iter = proto_array_.begin();
  RepeatedPtrField<string>::const_iterator c_iter = iter;
}

TEST_F(RepeatedPtrFieldIteratorTest, MutableIteration) {
  RepeatedPtrField<string>::iterator iter = proto_array_.begin();
  EXPECT_EQ("foo", *iter);
  ++iter;
  EXPECT_EQ("bar", *(iter++));
  EXPECT_EQ("baz", *iter);
  ++iter;
  EXPECT_TRUE(proto_array_.end() == iter);
  EXPECT_EQ("baz", *(--proto_array_.end()));
}

TEST_F(RepeatedPtrFieldIteratorTest, ConstIteration) {
  const RepeatedPtrField<string>& const_proto_array = proto_array_;
  RepeatedPtrField<string>::const_iterator iter = const_proto_array.begin();
  EXPECT_EQ("foo", *iter);
  ++iter;
  EXPECT_EQ("bar", *(iter++));
  EXPECT_EQ("baz", *iter);
  ++iter;
  EXPECT_TRUE(const_proto_array.end() == iter);
  EXPECT_EQ("baz", *(--const_proto_array.end()));
}

TEST_F(RepeatedPtrFieldIteratorTest, RandomAccess) {
  RepeatedPtrField<string>::iterator iter = proto_array_.begin();
  RepeatedPtrField<string>::iterator iter2 = iter;
  ++iter2;
  ++iter2;
  EXPECT_TRUE(iter + 2 == iter2);
  EXPECT_TRUE(iter == iter2 - 2);
  EXPECT_EQ("baz", iter[2]);
  EXPECT_EQ("baz", *(iter + 2));
  EXPECT_EQ(3, proto_array_.end() - proto_array_.begin());
}

TEST_F(RepeatedPtrFieldIteratorTest, Comparable) {
  RepeatedPtrField<string>::const_iterator iter = proto_array_.begin();
  RepeatedPtrField<string>::const_iterator iter2 = iter + 1;
  EXPECT_TRUE(iter == iter);
  EXPECT_TRUE(iter != iter2);
  EXPECT_TRUE(iter < iter2);
  EXPECT_TRUE(iter <= iter2);
  EXPECT_TRUE(iter <= iter);
  EXPECT_TRUE(iter2 > iter);
  EXPECT_TRUE(iter2 >= iter);
  EXPECT_TRUE(iter >= iter);
}

// Uninitialized iterator does not point to any of the RepeatedPtrField.
TEST_F(RepeatedPtrFieldIteratorTest, UninitializedIterator) {
  RepeatedPtrField<string>::iterator iter;
  EXPECT_TRUE(iter != proto_array_.begin());
  EXPECT_TRUE(iter != proto_array_.begin() + 1);
  EXPECT_TRUE(iter != proto_array_.begin() + 2);
  EXPECT_TRUE(iter != proto_array_.begin() + 3);
  EXPECT_TRUE(iter != proto_array_.end());
}

TEST_F(RepeatedPtrFieldIteratorTest, STLAlgorithms_lower_bound) {
  proto_array_.Clear();
  proto_array_.Add()->assign("a");
  proto_array_.Add()->assign("c");
  proto_array_.Add()->assign("d");
  proto_array_.Add()->assign("n");
  proto_array_.Add()->assign("p");
  proto_array_.Add()->assign("x");
  proto_array_.Add()->assign("y");

  string v = "f";
  RepeatedPtrField<string>::const_iterator it =
      lower_bound(proto_array_.begin(), proto_array_.end(), v);

  EXPECT_EQ(*it, "n");
  EXPECT_TRUE(it == proto_array_.begin() + 3);
}

TEST_F(RepeatedPtrFieldIteratorTest, Mutation) {
  RepeatedPtrField<string>::iterator iter = proto_array_.begin();
  *iter = "qux";
  EXPECT_EQ("qux", proto_array_.Get(0));
}

// -------------------------------------------------------------------

class RepeatedPtrFieldPtrsIteratorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    proto_array_.Add()->assign("foo");
    proto_array_.Add()->assign("bar");
    proto_array_.Add()->assign("baz");
  }

  RepeatedPtrField<string> proto_array_;
};

TEST_F(RepeatedPtrFieldPtrsIteratorTest, ConvertiblePtr) {
  RepeatedPtrField<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
}

TEST_F(RepeatedPtrFieldPtrsIteratorTest, MutablePtrIteration) {
  RepeatedPtrField<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  EXPECT_EQ("foo", **iter);
  ++iter;
  EXPECT_EQ("bar", **(iter++));
  EXPECT_EQ("baz", **iter);
  ++iter;
  EXPECT_TRUE(proto_array_.pointer_end() == iter);
  EXPECT_EQ("baz", **(--proto_array_.pointer_end()));
}

TEST_F(RepeatedPtrFieldPtrsIteratorTest, RandomPtrAccess) {
  RepeatedPtrField<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  RepeatedPtrField<string>::pointer_iterator iter2 = iter;
  ++iter2;
  ++iter2;
  EXPECT_TRUE(iter + 2 == iter2);
  EXPECT_TRUE(iter == iter2 - 2);
  EXPECT_EQ("baz", *iter[2]);
  EXPECT_EQ("baz", **(iter + 2));
  EXPECT_EQ(3, proto_array_.end() - proto_array_.begin());
}

TEST_F(RepeatedPtrFieldPtrsIteratorTest, ComparablePtr) {
  RepeatedPtrField<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  RepeatedPtrField<string>::pointer_iterator iter2 = iter + 1;
  EXPECT_TRUE(iter == iter);
  EXPECT_TRUE(iter != iter2);
  EXPECT_TRUE(iter < iter2);
  EXPECT_TRUE(iter <= iter2);
  EXPECT_TRUE(iter <= iter);
  EXPECT_TRUE(iter2 > iter);
  EXPECT_TRUE(iter2 >= iter);
  EXPECT_TRUE(iter >= iter);
}

// Uninitialized iterator does not point to any of the RepeatedPtrOverPtrs.
// Dereferencing an uninitialized iterator crashes the process.
TEST_F(RepeatedPtrFieldPtrsIteratorTest, UninitializedPtrIterator) {
  RepeatedPtrField<string>::pointer_iterator iter;
  EXPECT_TRUE(iter != proto_array_.pointer_begin());
  EXPECT_TRUE(iter != proto_array_.pointer_begin() + 1);
  EXPECT_TRUE(iter != proto_array_.pointer_begin() + 2);
  EXPECT_TRUE(iter != proto_array_.pointer_begin() + 3);
  EXPECT_TRUE(iter != proto_array_.pointer_end());
}


// This comparison functor is required by the tests for RepeatedPtrOverPtrs.
// They operate on strings and need to compare strings as strings in
// any stl algorithm, even though the iterator returns a pointer to a string
// - i.e. *iter has type string*.
struct StringLessThan {
  bool operator()(const string* z, const string& y) {
    return *z < y;
  }
  bool operator()(const string* z, const string* y) {
    return *z < *y;
  }
};

TEST_F(RepeatedPtrFieldPtrsIteratorTest, PtrSTLAlgorithms_lower_bound) {
  proto_array_.Clear();
  proto_array_.Add()->assign("a");
  proto_array_.Add()->assign("c");
  proto_array_.Add()->assign("d");
  proto_array_.Add()->assign("n");
  proto_array_.Add()->assign("p");
  proto_array_.Add()->assign("x");
  proto_array_.Add()->assign("y");

  RepeatedPtrField<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  string v = "f";
  RepeatedPtrField<string>::pointer_iterator it =
      lower_bound(proto_array_.pointer_begin(), proto_array_.pointer_end(),
                  &v, StringLessThan());

  GOOGLE_CHECK(*it != NULL);

  EXPECT_EQ(**it, "n");
  EXPECT_TRUE(it == proto_array_.pointer_begin() + 3);
}

TEST_F(RepeatedPtrFieldPtrsIteratorTest, PtrMutation) {
  RepeatedPtrField<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  **iter = "qux";
  EXPECT_EQ("qux", proto_array_.Get(0));

  EXPECT_EQ("bar", proto_array_.Get(1));
  EXPECT_EQ("baz", proto_array_.Get(2));
  ++iter;
  delete *iter;
  *iter = new string("a");
  ++iter;
  delete *iter;
  *iter = new string("b");
  EXPECT_EQ("a", proto_array_.Get(1));
  EXPECT_EQ("b", proto_array_.Get(2));
}

TEST_F(RepeatedPtrFieldPtrsIteratorTest, Sort) {
  proto_array_.Add()->assign("c");
  proto_array_.Add()->assign("d");
  proto_array_.Add()->assign("n");
  proto_array_.Add()->assign("p");
  proto_array_.Add()->assign("a");
  proto_array_.Add()->assign("y");
  proto_array_.Add()->assign("x");
  EXPECT_EQ("foo", proto_array_.Get(0));
  EXPECT_EQ("n", proto_array_.Get(5));
  EXPECT_EQ("x", proto_array_.Get(9));
  sort(proto_array_.pointer_begin(),
       proto_array_.pointer_end(),
       StringLessThan());
  EXPECT_EQ("a", proto_array_.Get(0));
  EXPECT_EQ("baz", proto_array_.Get(2));
  EXPECT_EQ("y", proto_array_.Get(9));
}


// -----------------------------------------------------------------------------
// Unit-tests for the insert iterators
// google::protobuf::RepeatedFieldBackInserter,
// google::protobuf::AllocatedRepeatedPtrFieldBackInserter
// Ported from util/gtl/proto-array-iterators_unittest.

class RepeatedFieldInsertionIteratorsTest : public testing::Test {
 protected:
  std::list<double> halves;
  std::list<int> fibonacci;
  std::vector<string> words;
  typedef TestAllTypes::NestedMessage Nested;
  Nested nesteds[2];
  std::vector<Nested*> nested_ptrs;
  TestAllTypes protobuffer;

  virtual void SetUp() {
    fibonacci.push_back(1);
    fibonacci.push_back(1);
    fibonacci.push_back(2);
    fibonacci.push_back(3);
    fibonacci.push_back(5);
    fibonacci.push_back(8);
    std::copy(fibonacci.begin(), fibonacci.end(),
              RepeatedFieldBackInserter(protobuffer.mutable_repeated_int32()));

    halves.push_back(1.0);
    halves.push_back(0.5);
    halves.push_back(0.25);
    halves.push_back(0.125);
    halves.push_back(0.0625);
    std::copy(halves.begin(), halves.end(),
              RepeatedFieldBackInserter(protobuffer.mutable_repeated_double()));

    words.push_back("Able");
    words.push_back("was");
    words.push_back("I");
    words.push_back("ere");
    words.push_back("I");
    words.push_back("saw");
    words.push_back("Elba");
    std::copy(words.begin(), words.end(),
              RepeatedFieldBackInserter(protobuffer.mutable_repeated_string()));

    nesteds[0].set_bb(17);
    nesteds[1].set_bb(4711);
    std::copy(&nesteds[0], &nesteds[2],
              RepeatedFieldBackInserter(
                  protobuffer.mutable_repeated_nested_message()));

    nested_ptrs.push_back(new Nested);
    nested_ptrs.back()->set_bb(170);
    nested_ptrs.push_back(new Nested);
    nested_ptrs.back()->set_bb(47110);
    std::copy(nested_ptrs.begin(), nested_ptrs.end(),
              RepeatedFieldBackInserter(
                  protobuffer.mutable_repeated_nested_message()));

  }

  virtual void TearDown() {
    STLDeleteContainerPointers(nested_ptrs.begin(), nested_ptrs.end());
  }
};

TEST_F(RepeatedFieldInsertionIteratorsTest, Fibonacci) {
  EXPECT_TRUE(std::equal(fibonacci.begin(),
                         fibonacci.end(),
                         protobuffer.repeated_int32().begin()));
  EXPECT_TRUE(std::equal(protobuffer.repeated_int32().begin(),
                         protobuffer.repeated_int32().end(),
                         fibonacci.begin()));
}

TEST_F(RepeatedFieldInsertionIteratorsTest, Halves) {
  EXPECT_TRUE(std::equal(halves.begin(),
                         halves.end(),
                         protobuffer.repeated_double().begin()));
  EXPECT_TRUE(std::equal(protobuffer.repeated_double().begin(),
                         protobuffer.repeated_double().end(),
                         halves.begin()));
}

TEST_F(RepeatedFieldInsertionIteratorsTest, Words) {
  ASSERT_EQ(words.size(), protobuffer.repeated_string_size());
  EXPECT_EQ(words.at(0), protobuffer.repeated_string(0));
  EXPECT_EQ(words.at(1), protobuffer.repeated_string(1));
  EXPECT_EQ(words.at(2), protobuffer.repeated_string(2));
  EXPECT_EQ(words.at(3), protobuffer.repeated_string(3));
  EXPECT_EQ(words.at(4), protobuffer.repeated_string(4));
  EXPECT_EQ(words.at(5), protobuffer.repeated_string(5));
  EXPECT_EQ(words.at(6), protobuffer.repeated_string(6));
}

TEST_F(RepeatedFieldInsertionIteratorsTest, Nesteds) {
  ASSERT_EQ(protobuffer.repeated_nested_message_size(), 4);
  EXPECT_EQ(protobuffer.repeated_nested_message(0).bb(), 17);
  EXPECT_EQ(protobuffer.repeated_nested_message(1).bb(), 4711);
  EXPECT_EQ(protobuffer.repeated_nested_message(2).bb(), 170);
  EXPECT_EQ(protobuffer.repeated_nested_message(3).bb(), 47110);
}

TEST_F(RepeatedFieldInsertionIteratorsTest,
       AllocatedRepeatedPtrFieldWithStringIntData) {
  vector<Nested*> data;
  TestAllTypes goldenproto;
  for (int i = 0; i < 10; ++i) {
    Nested* new_data = new Nested;
    new_data->set_bb(i);
    data.push_back(new_data);

    new_data = goldenproto.add_repeated_nested_message();
    new_data->set_bb(i);
  }
  TestAllTypes testproto;
  copy(data.begin(), data.end(),
       AllocatedRepeatedPtrFieldBackInserter(
           testproto.mutable_repeated_nested_message()));
  EXPECT_EQ(testproto.DebugString(), goldenproto.DebugString());
}

TEST_F(RepeatedFieldInsertionIteratorsTest,
       AllocatedRepeatedPtrFieldWithString) {
  vector<string*> data;
  TestAllTypes goldenproto;
  for (int i = 0; i < 10; ++i) {
    string* new_data = new string;
    *new_data = "name-" + SimpleItoa(i);
    data.push_back(new_data);

    new_data = goldenproto.add_repeated_string();
    *new_data = "name-" + SimpleItoa(i);
  }
  TestAllTypes testproto;
  copy(data.begin(), data.end(),
       AllocatedRepeatedPtrFieldBackInserter(
           testproto.mutable_repeated_string()));
  EXPECT_EQ(testproto.DebugString(), goldenproto.DebugString());
}

}  // namespace

}  // namespace protobuf
}  // namespace google
