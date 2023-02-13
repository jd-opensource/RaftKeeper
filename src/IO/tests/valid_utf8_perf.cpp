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
#include <IO/WriteBufferValidUTF8.h>
#include <IO/WriteBufferFromString.h>
#include <Common/Stopwatch.h>
#include <string>
#include <iostream>

int main(int argc, char ** argv)
{
    try
    {
        int repeats = 1;
        if (argc >= 2)
            repeats = std::stol(argv[1]);

        std::string text((std::istreambuf_iterator<char>(std::cin)),
                          std::istreambuf_iterator<char>());

        std::cout << "Text length: " << text.size() << std::endl;

        Stopwatch timer;
        std::string str1;
        {
            RK::WriteBufferFromString simple_buf(str1);
            for (int i = 0; i < repeats; ++i)
            {
                simple_buf.write(text.data(), text.size());
            }
        }
        double t = timer.elapsedSeconds();
        std::cout << "Wrote to string in " << t << "s at " << text.size() / 1e6 * repeats / t << "MB/s." << std::endl;
        std::cout << "String length: " << str1.size() << "(" << (str1.size() == text.size() * repeats ? "as " : "un") << "expected)" << std::endl;

        timer.restart();

        std::string str2;
        {
            RK::WriteBufferFromString simple_buf(str2);
            for (int i = 0; i < repeats; ++i)
            {
                RK::WriteBufferValidUTF8 utf_buf(simple_buf);
                utf_buf.write(text.data(), text.size());
            }
        }
        t = timer.elapsedSeconds();
        std::cout << "Wrote to UTF8 in " << t << "s at " << text.size() / 1e6 * repeats / t << "MB/s." << std::endl;
        std::cout << "String length: " << str2.size() << "(" << (str2.size() == text.size() * repeats ? "as " : "un") << "expected)" << std::endl;
    }
    catch (const RK::Exception & e)
    {
        std::cerr << e.what() << ", " << e.displayText() << std::endl;
        return 1;
    }

    return 0;
}
