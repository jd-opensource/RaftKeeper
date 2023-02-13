/**
 * Copyright 2016-2023 ClickHouse, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <Core/Field.h>
#include <common/demangle.h>
#include <Common/NaNUtils.h>


class SipHash;


namespace RK
{

namespace ErrorCodes
{
    extern const int CANNOT_CONVERT_TYPE;
    extern const int LOGICAL_ERROR;
    extern const int NOT_IMPLEMENTED;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
// Just dont mess with it. If the redundant redeclaration is removed then ReaderHelpers.h should be included.
// This leads to Arena.h inclusion which has a problem with ASAN stuff included properly and messing macro definition
// which intefrers with... You dont want to know, really.
UInt128 stringToUUID(const String & str);
#pragma GCC diagnostic pop


/** StaticVisitor (and its descendants) - class with overloaded operator() for all types of fields.
  * You could call visitor for field using function 'applyVisitor'.
  * Also "binary visitor" is supported - its operator() takes two arguments.
  */
template <typename R = void>
struct StaticVisitor
{
    using ResultType = R;
};


/// F is template parameter, to allow universal reference for field, that is useful for const and non-const values.
template <typename Visitor, typename F>
auto applyVisitor(Visitor && visitor, F && field)
{
    return Field::dispatch(std::forward<Visitor>(visitor),
        std::forward<F>(field));
}

template <typename Visitor, typename F1, typename F2>
auto applyVisitor(Visitor && visitor, F1 && field1, F2 && field2)
{
    return Field::dispatch(
        [&field2, &visitor](auto & field1_value)
        {
            return Field::dispatch(
                [&field1_value, &visitor](auto & field2_value)
                {
                    return visitor(field1_value, field2_value);
                },
                std::forward<F2>(field2));
        },
        std::forward<F1>(field1));
}


/** Prints Field as literal in SQL query */
class FieldVisitorToString : public StaticVisitor<String>
{
public:
    String operator() (const Null & x) const;
    String operator() (const UInt64 & x) const;
    String operator() (const UInt128 & x) const;
    String operator() (const Int64 & x) const;
    String operator() (const Int128 & x) const;
    String operator() (const Float64 & x) const;
    String operator() (const String & x) const;
    String operator() (const Array & x) const;
    String operator() (const Tuple & x) const;
    String operator() (const Map & x) const;
    String operator() (const AggregateFunctionStateData & x) const;

    String operator() (const UInt256 & x) const;
    String operator() (const Int256 & x) const;
};


class FieldVisitorWriteBinary
{
public:
    void operator() (const Null & x, WriteBuffer & buf) const;
    void operator() (const UInt64 & x, WriteBuffer & buf) const;
    void operator() (const UInt128 & x, WriteBuffer & buf) const;
    void operator() (const Int64 & x, WriteBuffer & buf) const;
    void operator() (const Int128 & x, WriteBuffer & buf) const;
    void operator() (const Float64 & x, WriteBuffer & buf) const;
    void operator() (const String & x, WriteBuffer & buf) const;
    void operator() (const Array & x, WriteBuffer & buf) const;
    void operator() (const Tuple & x, WriteBuffer & buf) const;
    void operator() (const Map & x, WriteBuffer & buf) const;
    void operator() (const AggregateFunctionStateData & x, WriteBuffer & buf) const;

    void operator() (const UInt256 & x, WriteBuffer & buf) const;
    void operator() (const Int256 & x, WriteBuffer & buf) const;
};


/** Print readable and unique text dump of field type and value. */
class FieldVisitorDump : public StaticVisitor<String>
{
public:
    String operator() (const Null & x) const;
    String operator() (const UInt64 & x) const;
    String operator() (const UInt128 & x) const;
    String operator() (const Int64 & x) const;
    String operator() (const Int128 & x) const;
    String operator() (const Float64 & x) const;
    String operator() (const String & x) const;
    String operator() (const Array & x) const;
    String operator() (const Tuple & x) const;
    String operator() (const Map & x) const;
    String operator() (const AggregateFunctionStateData & x) const;

    String operator() (const UInt256 & x) const;
    String operator() (const Int256 & x) const;
};


/** Converts numeric value of any type to specified type. */
template <typename T>
class FieldVisitorConvertToNumber : public StaticVisitor<T>
{
public:
    T operator() (const Null &) const
    {
        throw Exception("Cannot convert NULL to " + demangle(typeid(T).name()), ErrorCodes::CANNOT_CONVERT_TYPE);
    }

    T operator() (const String &) const
    {
        throw Exception("Cannot convert String to " + demangle(typeid(T).name()), ErrorCodes::CANNOT_CONVERT_TYPE);
    }

    T operator() (const Array &) const
    {
        throw Exception("Cannot convert Array to " + demangle(typeid(T).name()), ErrorCodes::CANNOT_CONVERT_TYPE);
    }

    T operator() (const Tuple &) const
    {
        throw Exception("Cannot convert Tuple to " + demangle(typeid(T).name()), ErrorCodes::CANNOT_CONVERT_TYPE);
    }

    T operator() (const Map &) const
    {
        throw Exception("Cannot convert Map to " + demangle(typeid(T).name()), ErrorCodes::CANNOT_CONVERT_TYPE);
    }

    T operator() (const UInt64 & x) const { return T(x); }
    T operator() (const Int64 & x) const { return T(x); }
    T operator() (const Int128 & x) const { return T(x); }

    T operator() (const Float64 & x) const
    {
        if constexpr (!std::is_floating_point_v<T>)
        {
            if (!isFinite(x))
            {
                /// When converting to bool it's ok (non-zero converts to true, NaN including).
                if (std::is_same_v<T, bool>)
                    return true;

                /// Conversion of infinite values to integer is undefined.
                throw Exception("Cannot convert infinite value to integer type", ErrorCodes::CANNOT_CONVERT_TYPE);
            }
        }

        if constexpr (std::is_same_v<Decimal256, T>)
            return Int256(x);
        else
            return T(x);
    }

    T operator() (const UInt128 &) const
    {
        throw Exception("Cannot convert UInt128 to " + demangle(typeid(T).name()), ErrorCodes::CANNOT_CONVERT_TYPE);
    }

    T operator() (const AggregateFunctionStateData &) const
    {
        throw Exception("Cannot convert AggregateFunctionStateData to " + demangle(typeid(T).name()), ErrorCodes::CANNOT_CONVERT_TYPE);
    }

    template <typename U, typename = std::enable_if_t<is_big_int_v<U>> >
    T operator() (const U & x) const
    {
        if constexpr (IsDecimalNumber<T>)
            return static_cast<T>(static_cast<typename T::NativeType>(x));
        else if constexpr (std::is_same_v<T, UInt128>)
            throw Exception("No conversion to old UInt128 from " + demangle(typeid(U).name()), ErrorCodes::NOT_IMPLEMENTED);
        else
            return static_cast<T>(x);
    }
};


/** Updates SipHash by type and value of Field */
class FieldVisitorHash : public StaticVisitor<>
{
private:
    SipHash & hash;
public:
    FieldVisitorHash(SipHash & hash_);

    void operator() (const Null & x) const;
    void operator() (const UInt64 & x) const;
    void operator() (const UInt128 & x) const;
    void operator() (const Int64 & x) const;
    void operator() (const Int128 & x) const;
    void operator() (const Float64 & x) const;
    void operator() (const String & x) const;
    void operator() (const Array & x) const;
    void operator() (const Tuple & x) const;
    void operator() (const Map & x) const;
    void operator() (const AggregateFunctionStateData & x) const;

    void operator() (const UInt256 & x) const;
    void operator() (const Int256 & x) const;
};


template <typename T> constexpr bool isDecimalField() { return false; }


/** Implements `+=` operation.
 *  Returns false if the result is zero.
 */
class FieldVisitorSum : public StaticVisitor<bool>
{
private:
    const Field & rhs;
public:
    explicit FieldVisitorSum(const Field & rhs_) : rhs(rhs_) {}

    // We can add all ints as unsigned regardless of their actual signedness.
    bool operator() (Int64 & x) const { return this->operator()(reinterpret_cast<UInt64 &>(x)); }
    bool operator() (UInt64 & x) const
    {
        x += rhs.reinterpret<UInt64>();
        return x != 0;
    }

    bool operator() (Float64 & x) const { x += get<Float64>(rhs); return x != 0; }

    bool operator() (Null &) const { throw Exception("Cannot sum Nulls", ErrorCodes::LOGICAL_ERROR); }
    bool operator() (String &) const { throw Exception("Cannot sum Strings", ErrorCodes::LOGICAL_ERROR); }
    bool operator() (Array &) const { throw Exception("Cannot sum Arrays", ErrorCodes::LOGICAL_ERROR); }
    bool operator() (Tuple &) const { throw Exception("Cannot sum Tuples", ErrorCodes::LOGICAL_ERROR); }
    bool operator() (Map &) const { throw Exception("Cannot sum Maps", ErrorCodes::LOGICAL_ERROR); }
    bool operator() (UInt128 &) const { throw Exception("Cannot sum UUIDs", ErrorCodes::LOGICAL_ERROR); }
    bool operator() (AggregateFunctionStateData &) const { throw Exception("Cannot sum AggregateFunctionStates", ErrorCodes::LOGICAL_ERROR); }

    bool operator() (Int128 & x) const
    {
        x += get<Int128>(rhs);
        return x != Int128(0);
    }

    template <typename T, typename = std::enable_if_t<is_big_int_v<T>> >
    bool operator() (T & x) const
    {
        x += rhs.reinterpret<T>();
        return x != T(0);
    }
};

}
