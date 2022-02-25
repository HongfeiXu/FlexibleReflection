#include <vector>
#include <iostream>
#include <string>
#include <cstddef>
#include <unordered_set>
#include <memory>

namespace reflect {

//--------------------------------------------------------
// Base class of all type descriptors
//--------------------------------------------------------

struct TypeDescriptor {
    const char* name;
    size_t size;

    TypeDescriptor(const char* name, size_t size) : name{name}, size{size} {}
    virtual ~TypeDescriptor() {}
    virtual std::string getFullName() const { return name; }
    virtual void dump(const void* obj, int indentLevel = 0) const = 0;
};

//--------------------------------------------------------
// Finding type descriptors
//--------------------------------------------------------

// Declare the function template that handles primitive types such as int, std::string, etc.:
template <typename T>
TypeDescriptor* getPrimitiveDescriptor();

// A helper class to find TypeDescriptors in different ways:
struct DefaultResolver {
    template <typename T> static char func(decltype(&T::Reflection));
    template <typename T> static int func(...);
    template <typename T>
    struct IsReflected {
        enum { value = (sizeof(func<T>(nullptr)) == sizeof(char)) };
    };

    // This version is called if T has a static member named "Reflection":
    template <typename T, typename std::enable_if<IsReflected<T>::value, int>::type = 0>
    static TypeDescriptor* get() {
        return &T::Reflection;
    }

    // This version is called otherwise:
    template <typename T, typename std::enable_if<!IsReflected<T>::value, int>::type = 0>
    static TypeDescriptor* get() {
        return getPrimitiveDescriptor<T>();
    }
};

// This is the primary class template for finding all TypeDescriptors:
template <typename T>
struct TypeResolver {
    static TypeDescriptor* get() {
        return DefaultResolver::get<T>();
    }
};

//--------------------------------------------------------
// Type descriptors for user-defined structs/classes
//--------------------------------------------------------

struct TypeDescriptor_Struct : TypeDescriptor {
    struct Member {
        const char* name;
        size_t offset;
        TypeDescriptor* type;
    };

    std::vector<Member> members;    // holds information about every reflected member of this struct

    /// <summary>
    /// ctor
    /// </summary>
    /// <param name="init">a pointer to init function</param>
    TypeDescriptor_Struct(void (*init)(TypeDescriptor_Struct*)) : TypeDescriptor{nullptr, 0} {
        init(this);
    }
    // 问：这个函数是干嘛的？？？
    TypeDescriptor_Struct(const char* name, size_t size, const std::initializer_list<Member>& init) : TypeDescriptor{nullptr, 0}, members{init} {
    }
    virtual void dump(const void* obj, int indentLevel) const override {
        std::cout << name << " {" << std::endl;
        for (const Member& member : members) {
            std::cout << std::string(4 * (indentLevel + 1), ' ') << member.name << " = ";
            member.type->dump((char*) obj + member.offset, indentLevel + 1);    // 传入每个member的地址，dump 里面按照类型去 cast 这个指针，获取对象
            std::cout << std::endl;
        }
        std::cout << std::string(4 * indentLevel, ' ') << "}";
    }
};

#define REFLECT() \
    friend struct reflect::DefaultResolver; \
    static reflect::TypeDescriptor_Struct Reflection; \
    static void initReflection(reflect::TypeDescriptor_Struct*);

#define REFLECT_STRUCT_BEGIN(type) \
    reflect::TypeDescriptor_Struct type::Reflection{type::initReflection}; \
    void type::initReflection(reflect::TypeDescriptor_Struct* typeDesc) { \
        using T = type; \
        typeDesc->name = #type; \
        typeDesc->size = sizeof(T); \
        typeDesc->members = {

#define REFLECT_STRUCT_MEMBER(name) \
            {#name, offsetof(T, name), reflect::TypeResolver<decltype(T::name)>::get()},

#define REFLECT_STRUCT_END() \
        }; \
    }

//--------------------------------------------------------
// Type descriptors for std::vector
//--------------------------------------------------------

struct TypeDescriptor_StdVector : TypeDescriptor {
    TypeDescriptor* itemType;           // a pointer to the type descriptor of its item type
    size_t (*getSize)(const void*);                 // 函数指针，指向的具有 size_t(const void*) 调用形式的函数、lambda
    const void* (*getItem)(const void*, size_t);    // 在函数模板内赋值 

    // Template constructor:
    template <typename ItemType>
    TypeDescriptor_StdVector(ItemType* /* dummy argument */)
        : TypeDescriptor{"std::vector<>", sizeof(std::vector<ItemType>)},
                         itemType{TypeResolver<ItemType>::get()} {
        // because the lambda expression is evaluated inside a template constructor, 
        // the compiler will generate a different anonymous function for each specialization of std::unique_ptr<>.
        getSize = [](const void* vecPtr) -> size_t {
            const auto& vec = *(const std::vector<ItemType>*) vecPtr;
            return vec.size();
        };
        getItem = [](const void* vecPtr, size_t index) -> const void* {
            const auto& vec = *(const std::vector<ItemType>*) vecPtr;
            return &vec[index];
        };
    }
    virtual std::string getFullName() const override {
        return std::string("std::vector<") + itemType->getFullName() + ">";
    }
    virtual void dump(const void* obj, int indentLevel) const override {
        size_t numItems = getSize(obj);
        std::cout << getFullName();
        if (numItems == 0) {
            std::cout << "{}";
        } else {
            std::cout << "{" << std::endl;
            for (size_t index = 0; index < numItems; index++) {
                std::cout << std::string(4 * (indentLevel + 1), ' ') << "[" << index << "] ";
                itemType->dump(getItem(obj, index), indentLevel + 1);
                std::cout << std::endl;
            }
            std::cout << std::string(4 * indentLevel, ' ') << "}";
        }
    }
};

// Partially specialize TypeResolver<> for std::vectors:
template <typename T>
class TypeResolver<std::vector<T>> {
public:
    static TypeDescriptor* get() {
        // 声明为静态变量，是因为我们对于相同类型（如 std::vector<int>）的不同变量，只需要一个 TypeDescriptor 即可
        static TypeDescriptor_StdVector typeDesc{(T*) nullptr};
        return &typeDesc;
    }
};

//--------------------------------------------------------
// Type descriptors for std::unordered_set
//--------------------------------------------------------

struct TypeDescriptor_StdUnorderedSet : TypeDescriptor {
    TypeDescriptor* itemType;
    size_t(*getSize)(const void*);
    void (*dumpUtil)(const void*, int);

    // Template constructor:
    template <typename ItemType>
    TypeDescriptor_StdUnorderedSet(ItemType* /* dummy argument */)
        : TypeDescriptor{"std::unordered_set", sizeof(std::unordered_set<ItemType>)},
          itemType {TypeResolver<ItemType>::get()} 
    {
        getSize = [](const void* unorderedSetPtr) -> size_t {
            const auto& unorderedSet = *(std::unordered_set<ItemType>*) unorderedSetPtr;
            return unorderedSet.size();
        };

        dumpUtil = [](const void* unorderedSetPtr, int indentLevel) -> void {
            TypeDescriptor* itemType = TypeResolver<ItemType>::get();
            const auto& unorderedSet = *(std::unordered_set<ItemType>*) unorderedSetPtr;
            size_t numItems = unorderedSet.size();
            std::cout << std::string("std::unordered_set<") + itemType->getFullName() + ">";
            if (numItems == 0) {
                std::cout << "{}";
            }
            else {
                std::cout << "{" << std::endl;
                for (const auto & item : unorderedSet) {
                    std::cout << std::string(4 * (indentLevel + 1), ' ');
                    itemType->dump(&item, indentLevel + 1);
                    std::cout << std::endl;
                }
                std::cout << std::string(4 * indentLevel, ' ') << "}";
            }
        };
    }

    virtual std::string getFullName() const override {
        return std::string("std::unordered_set<") + itemType->getFullName() + ">";
    }

    virtual void dump(const void* obj, int indentLevel) const override
    {
        dumpUtil(obj, indentLevel);
    }
};

// Partially specialize TypeResolver<> for std::unordered_set:
template <typename T>
class TypeResolver<std::unordered_set<T>> {
public:
    static TypeDescriptor* get() {
        static TypeDescriptor_StdUnorderedSet typeDesc{ (T*) nullptr };
        return &typeDesc;
    }
};

//--------------------------------------------------------
// Type descriptors for std::unique_ptr
//--------------------------------------------------------

struct TypeDescriptor_StdUniquePtr : TypeDescriptor {
    TypeDescriptor* targetType;
    const void* (*getTarget)(const void*);

    template <typename TargetType>
    TypeDescriptor_StdUniquePtr(TargetType* /* dummy argument */)
        : TypeDescriptor{"std::unique_ptr", sizeof(std::unique_ptr<TargetType>)},
        targetType(TypeResolver<TargetType>::get()) {
        getTarget = [](const void* uniquePtrPtr) -> const void* {
            const auto& uniquePtr = *(std::unique_ptr<TargetType>*) uniquePtrPtr;
            return uniquePtr.get();
        };
    }

    virtual std::string getFullName() const override {
        return std::string("std::unique_ptr<") + targetType->getFullName() + ">";
    }

    virtual void dump(const void* obj, int indentLevel) const override
    {
        std::cout << getFullName() << "{";
        const void* targetObj = getTarget(obj);
        if (targetObj == nullptr) {
            std::cout << "nullptr";
        }
        else {
            std::cout << "\n";
            std::cout << std::string(4 * (indentLevel + 1), ' ');
            targetType->dump(targetObj, indentLevel + 1);
            std::cout << "\n";
            std::cout << std::string(4 * indentLevel, ' ');
        }
        std::cout << "}";
    }
};

// Partially specialize TypeResolver<> for std::unique_ptr:
template <typename T>
class TypeResolver<std::unique_ptr<T>> {
public:
    static TypeDescriptor* get() {
        static TypeDescriptor_StdUniquePtr typeDesc{ (T*) nullptr };
        return &typeDesc;
    }
};

} // namespace reflect
