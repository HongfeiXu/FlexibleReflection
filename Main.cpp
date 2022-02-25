#include "Reflect.h"

struct Node {
    std::string key;
    int value;
    std::vector<Node> children;

    //REFLECT()       // Enable reflection for this type

    // --> 

    friend struct reflect::DefaultResolver;
    // Declare the struct's type descriptor:
    // static member variable, its constructor is automatically called at program startup, before main
    static reflect::TypeDescriptor_Struct Reflection;
    // Declare a function to initialize it:
    static void initReflection(reflect::TypeDescriptor_Struct*);
};

struct Cute {
    std::string key;
    double value;
    std::unique_ptr<Node> next;

    REFLECT()
};

int main() {
    std::cout << "---main---" << std::endl;

    // Cute
    Cute cute = {
        "apple",
        5,
        std::unique_ptr<Node>{ 
            new Node{
                "banana", 
                5, 
                {{"hel", 1, {}}}
            }
        } 
    };
    reflect::TypeDescriptor* typeDescForCute = reflect::TypeResolver<decltype(cute)>::get();
    typeDescForCute->dump(&cute);
    std::cout << "\n";

    // int
    int i = 109;
    reflect::TypeDescriptor* typeDescForInt = reflect::TypeResolver<decltype(i)>::get();
    typeDescForInt->dump(&i);
    std::cout << "\n";

    // double
    double d = 1.2;
    reflect::TypeDescriptor* typeDescForDouble = reflect::TypeResolver<decltype(d)>::get();
    typeDescForDouble->dump(&d);
    std::cout << "\n";

    // vector
    std::vector<double> vd{ 1.2,2.3,3.4 };
    reflect::TypeDescriptor* typeDescForVector = reflect::TypeResolver<decltype(vd)>::get();
    typeDescForVector->dump(&vd);
    std::cout << "\n";

    // unordered_set
    std::unordered_set<double> us{ 1.2,2,3 };
    reflect::TypeDescriptor* typeDescForUnorderedSet = reflect::TypeResolver<decltype(us)>::get();
    typeDescForUnorderedSet->dump(&us);
    std::cout << "\n";

    // Create an object of type Node
    Node node = {"apple", 3, {{"boomb", 7, {}}, {"cherry", 11, {}}}};
    // Find Node's type descriptor
    reflect::TypeDescriptor* typeDesc = reflect::TypeResolver<decltype(node)>::get();
    // Dump a description of the Node object to the console
    typeDesc->dump(&node);

    return 0;
}


// Define Node's type descriptor
//REFLECT_STRUCT_BEGIN(Node)
//REFLECT_STRUCT_MEMBER(key)
//REFLECT_STRUCT_MEMBER(value)
//REFLECT_STRUCT_MEMBER(children)
//REFLECT_STRUCT_END()

// -->

// Definition of the struct's type descriptor:
reflect::TypeDescriptor_Struct Node::Reflection{ Node::initReflection };

// Definition of the function that initializes it:
void Node::initReflection(reflect::TypeDescriptor_Struct* typeDesc) {
    std::cout << "---initReflection before main---" << std::endl;
    using T = Node;
    typeDesc->name = "Node";
    typeDesc->size = sizeof(T);
    typeDesc->members = {
        // reflect::TypeResolver<>::get() finds the type descriptor for Node::key
        {"key", offsetof(T, key), reflect::TypeResolver<decltype(T::key)>::get()},  // 列表初始化 member
        // reflect::TypeResolver<>::get() finds the type descriptor for Node::value
        {"value", offsetof(T, value), reflect::TypeResolver<decltype(T::value)>::get()},
        // reflect::TypeResolver<>::get() finds the type descriptor for Node::children
        {"children", offsetof(T, children), reflect::TypeResolver<decltype(T::children)>::get()},
    };
}

// notice:
// 
// 1. offsetof 
// If type is not a PODType (until C++11)standard layout type (since C++11), 
// the behavior is undefined (until C++17)use of the offsetof macro is conditionally-supported (since C++17).


REFLECT_STRUCT_BEGIN(Cute)
REFLECT_STRUCT_MEMBER(key)
REFLECT_STRUCT_MEMBER(value)
REFLECT_STRUCT_MEMBER(next)
REFLECT_STRUCT_END()

