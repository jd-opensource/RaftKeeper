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

#include <boost/noncopyable.hpp>

#include <Common/HyperLogLogCounter.h>
#include <Common/HashTable/SmallTable.h>


namespace RK
{


/** For a small number of keys - an array of fixed size "on the stack".
  * For large, HyperLogLog is allocated.
  * See also the more practical implementation in CombinedCardinalityEstimator.h,
  *  where a hash table is also used for medium-sized sets.
  */
template
<
    typename Key,
    UInt8 small_set_size,
    UInt8 K,
    typename Hash = IntHash32<Key>,
    typename DenominatorType = double>
class HyperLogLogWithSmallSetOptimization : private boost::noncopyable
{
private:
    using Small = SmallSet<Key, small_set_size>;
    using Large = HyperLogLogCounter<K, Hash, UInt32, DenominatorType>;
    using LargeValueType = typename Large::value_type;

    Small small;
    Large * large = nullptr;

    bool isLarge() const
    {
        return large != nullptr;
    }

    void toLarge()
    {
        /// At the time of copying data from `tiny`, setting the value of `large` is still not possible (otherwise it will overwrite some data).
        Large * tmp_large = new Large;

        for (const auto & x : small)
            tmp_large->insert(static_cast<LargeValueType>(x.getValue()));

        large = tmp_large;
    }

public:
    using value_type = Key;

    ~HyperLogLogWithSmallSetOptimization()
    {
        if (isLarge())
            delete large;
    }

    /// ALWAYS_INLINE is required to have better code layout for uniqHLL12 function
    void ALWAYS_INLINE insert(Key value)
    {
        if (!isLarge())
        {
            if (small.find(value) == small.end())
            {
                if (!small.full())
                    small.insert(value);
                else
                {
                    toLarge();
                    large->insert(static_cast<LargeValueType>(value));
                }
            }
        }
        else
            large->insert(static_cast<LargeValueType>(value));
    }

    UInt64 size() const
    {
        return !isLarge() ? small.size() : large->size();
    }

    void merge(const HyperLogLogWithSmallSetOptimization & rhs)
    {
        if (rhs.isLarge())
        {
            if (!isLarge())
                toLarge();

            large->merge(*rhs.large);
        }
        else
        {
            for (const auto & x : rhs.small)
                insert(x.getValue());
        }
    }

    /// You can only call for an empty object.
    void read(RK::ReadBuffer & in)
    {
        bool is_large;
        readBinary(is_large, in);

        if (is_large)
        {
            toLarge();
            large->read(in);
        }
        else
            small.read(in);
    }

    void readAndMerge(RK::ReadBuffer & in)
    {
        bool is_rhs_large;
        readBinary(is_rhs_large, in);

        if (!isLarge() && is_rhs_large)
            toLarge();

        if (!is_rhs_large)
        {
            typename Small::Reader reader(in);
            while (reader.next())
                insert(reader.get());
        }
        else
            large->readAndMerge(in);
    }

    void write(RK::WriteBuffer & out) const
    {
        writeBinary(isLarge(), out);

        if (isLarge())
            large->write(out);
        else
            small.write(out);
    }
};


}
