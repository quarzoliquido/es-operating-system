/*
 * Copyright 2008, 2009 Google Inc.
 * Copyright 2007 Nintendo Co., Ltd.
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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "esidl.h"

class Cxx : public Visitor
{
    std::string indent;
    std::string prefix;
    FILE* file;
    bool interfaceMode;
    bool constructorMode;

    void printChildren(const Node* node)
    {
        if (node->isLeaf())
        {
            return;
        }

        std::string separator;
        bool br;
        int count = 0;
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            if (1 < (*i)->getRank())
            {
                continue;
            }
            if ((*i)->isNative(node->getParent()))
            {
                continue;
            }
            if (0 < count)
            {
                fprintf(file, "%s", separator.c_str());
            }
            separator = (*i)->getSeparator();
            br  = (separator[separator.size() - 1] == '\n') ? true : false;
            if (br)
            {
                fprintf(file, "%s", indent.c_str());
            }
            if (0 < prefix.size())
            {
                fprintf(file, "%s", prefix.c_str());
            }
            (*i)->accept(this);
            ++count;
        }
        if (br && 0 < count)
        {
            fprintf(file, "%s", separator.c_str());
        }
    }

public:
    Cxx(FILE* file) :
        file(file),
        interfaceMode(false),
        constructorMode(false)
    {
    }

    virtual void at(const Node* node)
    {
        if (0 < node->getName().size())
        {
            if (!interfaceMode)
            {
                fprintf(file, "%s", node->getName().c_str());
            }
            else
            {
                std::string name = node->getName();
                size_t pos = name.rfind("::");
                if (pos != std::string::npos)
                {
                    name.insert(pos + 2, "I");
                    fprintf(file, "%s", name.c_str());
                }
                else
                {
                    fprintf(file, "I%s", name.c_str());
                }
            }
        }
        else
        {
            printChildren(node);
        }
    }

    virtual void at(const Module* node)
    {
        if (0 < node->getName().size())
        {
            if (node->getJavadoc().size())
            {
                fprintf(file, "%s\n%s", node->getJavadoc().c_str(), indent.c_str());
            }
            fprintf(file, "namespace %s\n", node->getName().c_str());
            fprintf(file, "%s{\n", indent.c_str());
            indent += "    ";
            printChildren(node);
            indent.erase(indent.length() - 4);
            fprintf(file, "%s}", indent.c_str());
        }
        else
        {
            printChildren(node);
        }
    }

    virtual void at(const EnumType* node)
    {
        fprintf(file, "enum %s", node->getName().c_str());
        fprintf(file, "\n%s{\n", indent.c_str());
        indent += "    ";
        fprintf(file, "%s", indent.c_str());
        printChildren(node);
        indent.erase(indent.length() - 4);
        fprintf(file, "\n%s}", indent.c_str());
    }

    virtual void at(const StructType* node)
    {
        if (node->getJavadoc().size())
        {
            fprintf(file, "%s\n%s", node->getJavadoc().c_str(), indent.c_str());
        }
        fprintf(file, "struct %s", node->getName().c_str());
        if (!node->isLeaf())
        {
            fprintf(file, "\n%s{\n", indent.c_str());
            indent += "    ";
            printChildren(node);
            indent.erase(indent.length() - 4);
            fprintf(file, "%s}", indent.c_str());
        }
    }

    virtual void at(const ExceptDcl* node)
    {
        if (node->getJavadoc().size())
        {
            fprintf(file, "%s\n%s", node->getJavadoc().c_str(), indent.c_str());
        }
        fprintf(file, "struct %s", node->getName().c_str());
        fprintf(file, "\n%s{\n", indent.c_str());
        indent += "    ";
        printChildren(node);
        indent.erase(indent.length() - 4);
        fprintf(file, "%s}", indent.c_str());
    }

    virtual void at(const Interface* node)
    {
        if (node->getJavadoc().size())
        {
            fprintf(file, "%s\n%s", node->getJavadoc().c_str(), indent.c_str());
        }
        fprintf(file, "class I%s", node->getName().c_str());
        if (node->getExtends())
        {
            fprintf(file, " : ");
            prefix = "public ";
            interfaceMode = true;
            node->getExtends()->accept(this);
            interfaceMode = false;
            prefix = "";
        }
        if (!node->isLeaf())
        {
            fprintf(file, "\n%s{\n%spublic:\n", indent.c_str(), indent.c_str());
            indent += "    ";

            // printChildren(node);
            int count = 0;
            for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
            {
                if (dynamic_cast<PragmaID*>(*i) || (*i)->isSequence(node) || (*i)->isNative(node))
                {
                    continue;
                }

                if (0 < count)
                {
                    fprintf(file, ";\n");
                }
                fprintf(file, "%s", indent.c_str());

                ++count;
                (*i)->accept(this);
            }
            if (0 < count)
            {
                fprintf(file, ";\n");
            }

            fprintf(file, "%sstatic const char* iid()\n", indent.c_str());
            fprintf(file, "%s{\n", indent.c_str());
            fprintf(file, "%s    static const char* name = \"%s\";\n", indent.c_str(), node->getFullyQualifiedName().c_str());
            fprintf(file, "%s    return name;\n", indent.c_str());
            fprintf(file, "%s}\n", indent.c_str());

            if (Interface* constructor = node->getConstructor())
            {
                // Process constructors.
                constructorMode = true;
                for (NodeList::iterator i = constructor->begin();
                     i != constructor->end();
                     ++i)
                {
                    fprintf(file, "%s", indent.c_str());
                    (*i)->accept(this);
                }
                constructorMode = false;
                fprintf(file, "%sstatic IConstructor* getConstructor()\n", indent.c_str());
                fprintf(file, "%s{\n", indent.c_str());
                fprintf(file, "%s    return constructor;\n", indent.c_str());
                fprintf(file, "%s}\n", indent.c_str());
                fprintf(file, "%sstatic void setConstructor(IConstructor* ctor)\n", indent.c_str());
                fprintf(file, "%s{\n", indent.c_str());
                fprintf(file, "%s    constructor = ctor;\n", indent.c_str());
                fprintf(file, "%s}\n", indent.c_str());
                fprintf(file, "private:\n");
                fprintf(file, "%sstatic IConstructor* constructor;\n", indent.c_str());
            }

            indent.erase(indent.length() - 4);
            fprintf(file, "%s}", indent.c_str());
        }

        if (node->getConstructor())
        {
            fprintf(file, ";\n\n");
            fprintf(file, "%sI%s::IConstructor* I%s::constructor __attribute__((weak))",
                    indent.c_str(), node->getName().c_str(), node->getName().c_str());

        }
    }

    virtual void at(const Type* node)
    {
        if (node->getName() == "boolean")
        {
            fprintf(file, "bool");
        }
        else if (node->getName() == "octet")
        {
            fprintf(file, "unsigned char");
        }
        else if (node->getName() == "long")
        {
            fprintf(file, "int");
        }
        else if (node->getName() == "unsigned long")
        {
            fprintf(file, "unsigned int");
        }
        else if (node->getName() == "any")
        {
            fprintf(file, "Any");
        }
        else if (node->getName() == "wchar")
        {
            fprintf(file, "wchar_t");
        }
        else if (node->getName() == "string")
        {
            fprintf(file, "char*");
        }
        else if (node->getName() == "wstring")
        {
            fprintf(file, "wchar_t*");
        }
        else if (node->getName() == "Object")
        {
            fprintf(file, "void*");
        }
        else if (node->getName() == "uuid")
        {
            fprintf(file, "Guid&");
        }
        else
        {
            fprintf(file, "%s", node->getName().c_str());
        }
    }

    virtual void at(const NativeType* node)
    {
        if (node->getName() == "void_pointer")
        {
            fprintf(file, "void*");
        }
        else
        {
            fprintf(file, "%s", node->getName().c_str());
        }
    }

    virtual void at(const SequenceType* node)
    {
        Node* spec = node->getSpec();
        if (spec->isOctet(node->getParent()))
        {
            fprintf(file, "void*");
        }
        else
        {
            spec->accept(this);
            fprintf(file, "*");
        }
    }

    virtual void at(const BinaryExpr* node)
    {
        node->getLeft()->accept(this);
        fprintf(file, " %s ", node->getName().c_str());
        node->getRight()->accept(this);
    }

    virtual void at(const UnaryExpr* node)
    {
        fprintf(file, "%s", node->getName().c_str());
        NodeList::iterator elem = node->begin();
        (*elem)->accept(this);
    }

    virtual void at(const GroupingExpression* node)
    {
        fprintf(file, "(");
        NodeList::iterator elem = node->begin();
        (*elem)->accept(this);
        fprintf(file, ")");
    }

    virtual void at(const Literal* node)
    {
        if (node->getName() == "TRUE")
        {
            fprintf(file, "true");
        }
        else if (node->getName() == "FALSE")
        {
            fprintf(file, "false");
        }
        else
        {
            fprintf(file, "%s", node->getName().c_str());
        }
    }

    virtual void at(const PragmaID* node)
    {
    }

    virtual void at(const Member* node)
    {
        if (node->isTypedef())
        {
            fprintf(file, "typedef ");
        }
        node->getSpec()->accept(this);
        fprintf(file, " %s", node->getName().c_str());
    }

    virtual void at(const ArrayDcl* node)
    {
        assert(!node->isLeaf());

        at(static_cast<const Member*>(node));
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            fprintf(file, "[");
            (*i)->accept(this);
            fprintf(file, "]");
        }
    }

    virtual void at(const Attribute* node)
    {
        std::string cap = node->getName().c_str();
        cap[0] = toupper(cap[0]);   // XXX
        Node* spec = node->getSpec();
        SequenceType* seq = const_cast<SequenceType*>(spec->isSequence(node->getParent()));
        std::string name = node->getName();
        size_t pos = name.rfind("::");
        if (pos != std::string::npos)
        {
            name = name.substr(pos + 2);
        }
        name[0] = tolower(name[0]); // XXX

        if (node->getJavadoc().size())
        {
            fprintf(file, "%s\n%s", node->getJavadoc().c_str(), indent.c_str());
        }

        // getter
        fprintf(file, "virtual ");
        if (seq ||
            spec->isString(node->getParent()) ||
            spec->isWString(node->getParent()))
        {
            fprintf(file, "int get%s(", cap.c_str());
            if (seq)
            {
                seq->accept(this);
            }
            else
            {
                spec->accept(this);
            }
            fprintf(file, " %s, int %sLength)", name.c_str(), name.c_str());
        }
        else if (spec->isStruct(node->getParent()))
        {
            fprintf(file, "void get%s(", cap.c_str());
            spec->accept(this);
            fprintf(file, "* %s)", name.c_str());
        }
        else if (spec->isArray(node->getParent()))
        {
            fprintf(file, "void get%s(", cap.c_str());
            spec->accept(this);
            fprintf(file, " %s)", name.c_str());
        }
        else if (spec->isAny(node->getParent()))
        {
            spec->accept(this);
            fprintf(file, " get%s(", cap.c_str());
            fprintf(file, "void* %s, int %sLength)", name.c_str(), name.c_str());
        }
        else
        {
            if (spec->isInterface(node->getParent()))
            {
                interfaceMode = true;
                spec->accept(this);
                fprintf(file, "*");
                interfaceMode = false;
            }
            else if (NativeType* nativeType = spec->isNative(node->getParent()))
            {
                nativeType->accept(this);
            }
            else
            {
                spec->accept(this);
            }
            fprintf(file, " %s%s()", spec->isBoolean(node->getParent()) ? "is" : "get", cap.c_str());
        }
        fprintf(file, " = 0");

        if (!node->isReadonly())
        {
            fprintf(file, ";\n%s", indent.c_str());

            // setter
            fprintf(file, "virtual ");
            if (seq)
            {
                fprintf(file, "int set%s(const ", cap.c_str());
                seq->accept(this);
                fprintf(file, " %s, int %sLength)", name.c_str(), name.c_str());
            }
            else if (spec->isString(node->getParent()) || spec->isWString(node->getParent()))
            {
                fprintf(file, "int set%s(const ", cap.c_str());
                spec->accept(this);
                fprintf(file, " %s)", name.c_str());
            }
            else if (spec->isStruct(node->getParent()))
            {
                fprintf(file, "void set%s(const ", cap.c_str());
                spec->accept(this);
                fprintf(file, "* %s)", name.c_str());
            }
            else if (spec->isArray(node->getParent()) ||
                     spec->isAny(node->getParent()))
            {
                fprintf(file, "void set%s(const ", cap.c_str());
                spec->accept(this);
                fprintf(file, " %s)", name.c_str());
            }
            else
            {
                fprintf(file, "void set%s(", cap.c_str());
                if (spec->isInterface(node->getParent()))
                {
                    interfaceMode = true;
                    spec->accept(this);
                    fprintf(file, "*");
                    interfaceMode = false;
                }
                else if (NativeType* nativeType = spec->isNative(node->getParent()))
                {
                    nativeType->accept(this);
                }
                else
                {
                    spec->accept(this);
                }
                fprintf(file, " %s)", name.c_str());
            }
            fprintf(file, " = 0");
        }
    }

    virtual void at(const ConstDcl* node)
    {
        if (node->getJavadoc().size())
        {
            fprintf(file, "%s\n%s", node->getJavadoc().c_str(), indent.c_str());
        }
        fprintf(file, "static const ");
        at(static_cast<const Member*>(node));
        fprintf(file, " = ");
        node->getExp()->accept(this);
    }

    virtual void at(const OpDcl* node)
    {
        if (node->getJavadoc().size())
        {
            fprintf(file, "%s\n%s", node->getJavadoc().c_str(), indent.c_str());
        }
        if (!constructorMode)
        {
            fprintf(file, "virtual ");
        }
        else
        {
            fprintf(file, "static ");
        }

        Node* spec = node->getSpec();
        SequenceType* seq = const_cast<SequenceType*>(spec->isSequence(node->getParent()));
        if (seq ||
            spec->isString(node->getParent()) ||
            spec->isWString(node->getParent()))
        {
            std::string name = spec->getName();
            size_t pos = name.rfind("::");
            if (pos != std::string::npos)
            {
                name = name.substr(pos + 2);
            }
            name[0] = tolower(name[0]); // XXX

            fprintf(file, "int %s(", node->getName().c_str());
            if (seq)
            {
                seq->accept(this);
            }
            else
            {
                spec->accept(this);
            }
            fprintf(file, " %s, int %sLength", name.c_str(), name.c_str());

            if (node->begin() != node->end())
            {
                fprintf(file, ", ");
            }
        }
        else if (spec->isStruct(node->getParent()))
        {
            std::string name = spec->getName();
            size_t pos = name.rfind("::");
            if (pos != std::string::npos)
            {
                name = name.substr(pos + 2);
            }
            name[0] = tolower(name[0]); // XXX

            fprintf(file, "void %s(", node->getName().c_str());
            spec->accept(this);
            fprintf(file, "* %s", name.c_str());

            if (node->begin() != node->end())
            {
                fprintf(file, ", ");
            }
        }
        else if (spec->isArray(node->getParent()))
        {
            std::string name = spec->getName();
            size_t pos = name.rfind("::");
            if (pos != std::string::npos)
            {
                name = name.substr(pos + 2);
            }
            name[0] = tolower(name[0]); // XXX

            fprintf(file, "void %s(", node->getName().c_str());
            spec->accept(this);
            fprintf(file, " %s", name.c_str());

            if (node->begin() != node->end())
            {
                fprintf(file, ", ");
            }
        }
        else if (spec->isAny(node->getParent()))
        {
            std::string name = spec->getName();
            size_t pos = name.rfind("::");
            if (pos != std::string::npos)
            {
                name = name.substr(pos + 2);
            }
            name[0] = tolower(name[0]); // XXX

            spec->accept(this);
            fprintf(file, " %s(", node->getName().c_str());
            fprintf(file, "void* %s, int %sLength", name.c_str(), name.c_str());

            if (node->begin() != node->end())
            {
                fprintf(file, ", ");
            }
        }
        else
        {
            if (spec->isInterface(node->getParent()))
            {
                interfaceMode = true;
                spec->accept(this);
                fprintf(file, "*");
                interfaceMode = false;
            }
            else if (NativeType* nativeType = spec->isNative(node->getParent()))
            {
                nativeType->accept(this);
            }
            else
            {
                spec->accept(this);
            }
            fprintf(file, " %s", node->getName().c_str());
            fprintf(file, "(");
        }

        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            if (i != node->begin())
            {
                fprintf(file, ", ");
            }
            (*i)->accept(this);
        }

        fprintf(file, ")");
        if (node->getRaises())
        {
            fprintf(file, " throw(");
            node->getRaises()->accept(this);
            fprintf(file, ")");
        }

        if (!constructorMode)
        {
            fprintf(file, " = 0");
        }
        else
        {
            fprintf(file, "\n%s{\n", indent.c_str());
            fprintf(file, "%s    if (constructor)\n", indent.c_str());
            fprintf(file, "%s        constructor->createInstance(", indent.c_str());
            for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
            {
                if (i != node->begin())
                {
                    fprintf(file, ", ");
                }
                fprintf(file, "%s", (*i)->getName().c_str());
            }
            fprintf(file, ");\n");
            fprintf(file, "%s}\n", indent.c_str());
        }
    }

    virtual void at(const ParamDcl* node)
    {
        Node* spec = node->getSpec();
        SequenceType* seq = const_cast<SequenceType*>(spec->isSequence(node->getParent()));

        switch (node->getAttr())
        {
        case ParamDcl::In:
            if (seq ||
                spec->isGuid(node->getParent()) ||
                spec->isString(node->getParent()) ||
                spec->isWString(node->getParent()) ||
                spec->isStruct(node->getParent()) ||
                spec->isArray(node->getParent()))
            {
                fprintf(file, "const ");
            }
            break;
        }

        if (seq)
        {
            seq->accept(this);
            fprintf(file, " %s, int %sLength", node->getName().c_str() , node->getName().c_str());
        }
        else if (spec->isStruct(node->getParent()))
        {
            spec->accept(this);
            fprintf(file, "* %s", node->getName().c_str());
        }
        else if (spec->isArray(node->getParent()))
        {
            spec->accept(this);
            fprintf(file, " %s", node->getName().c_str());
        }
        else
        {
            if (spec->isInterface(node->getParent()))
            {
                interfaceMode = true;
                spec->accept(this);
                fprintf(file, "*");
                interfaceMode = false;
            }
            else if (NativeType* nativeType = spec->isNative(node->getParent()))
            {
                nativeType->accept(this);
            }
            else
            {
                spec->accept(this);
            }
            if (!spec->isString(node->getParent()) &&
                !spec->isWString(node->getParent()))
            {
                switch (node->getAttr())
                {
                case ParamDcl::Out:
                case ParamDcl::InOut:
                    fprintf(file, "*");
                    break;
                }
                fprintf(file, " %s", node->getName().c_str());
            }
            else
            {
                fprintf(file, " %s", node->getName().c_str());
                switch (node->getAttr())
                {
                case ParamDcl::Out:
                case ParamDcl::InOut:
                    fprintf(file, ", int %sLength", node->getName().c_str());
                    break;
                }
            }
        }
    }

    virtual void at(const Include* node)
    {
    }
};

std::string getIncludedName(const std::string& header)
{
    std::string included(header);

    for (int i = 0; i < included.size(); ++i)
    {
        char c = included[i];
        included[i] = toupper(c);
        if (c == '.' || c == '/' || c == '\\')
        {
            included[i] = '_';
        }
    }
    return included + "_INCLUDED";
}

class Import : public Visitor
{
    FILE* file;

public:
    Import(FILE* file) :
        file(file)
    {
    }

    virtual void at(const Node* node)
    {
        if (1 < node->getRank())
        {
            return;
        }
        visitChildren(node);
    }

    virtual void at(const Include* node)
    {
        if (1 < node->getRank())
        {
            return;
        }
        fprintf(file, "#include <%s>\n", getOutputFilename(node->getName().c_str(), "h").c_str());
    }
};

void printCxx(const std::string& filename)
{
    printf("# %s\n", filename.c_str());

    FILE* file = fopen(filename.c_str(), "w");
    if (!file)
    {
        return;
    }

    std::string included = getIncludedName(filename);
    fprintf(file, "// Generated by esidl %s.\n\n", VERSION);
    fprintf(file, "#ifndef %s\n", included.c_str());
    fprintf(file, "#define %s\n\n", included.c_str());

    Import import(file);
    getSpecification()->accept(&import);

    Cxx cxx(file);
    getSpecification()->accept(&cxx);

    fprintf(file, "#endif  // %s\n", included.c_str());

    fclose(file);
}
