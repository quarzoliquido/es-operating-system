/*
 * Copyright 2010 Esrille Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CharacterDataImp.h"

namespace org { namespace w3c { namespace dom { namespace bootstrap {

// Node
std::u16string CharacterDataImp::getTextContent()
{
    return getData();
}

void CharacterDataImp::setTextContent(std::u16string textContent)
{
    setData(textContent);
}

// CharacterData
std::u16string CharacterDataImp::getData() throw(DOMException)
{
    return data;
}

void CharacterDataImp::setData(std::u16string data) throw(DOMException)
{
    this->data = data;
}

unsigned int CharacterDataImp::getLength()
{
    return data.length();
}

std::u16string CharacterDataImp::substringData(unsigned int offset, unsigned int count) throw(DOMException)
{
    return data.substr(offset, count);
}

void CharacterDataImp::appendData(std::u16string arg) throw(DOMException)
{
    data += arg;
}

void CharacterDataImp::insertData(unsigned int offset, std::u16string arg) throw(DOMException)
{
    data.insert(offset, arg);
}

void CharacterDataImp::deleteData(unsigned int offset, unsigned int count) throw(DOMException)
{
    data.erase(offset, count);
}

void CharacterDataImp::replaceData(unsigned int offset, unsigned int count, std::u16string arg) throw(DOMException)
{
    data.replace(offset, count, arg);
}

}}}}  // org::w3c::dom::bootstrap