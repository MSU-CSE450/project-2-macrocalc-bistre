For future reference, here's what all the functions/classes in SymbolTable.hpp do and how we'd use them in the actual interpreter.
# Classes
The VariableInfo struct contains everything you'd expect (name, value, line number) plus an additional boolean variable, "initialized", which tracks whether the variable has been merely declared or has had some kind of value assigned to it. The idea is that this should tell us when to throw an error due to a variable being read before receiving a value. See the "how to use in the project" for more.

The SymbolTable class contains the stack of scopes (each of which is a map with strings as keys and size_t's as values) and a vector "all_variables" of VariableInfo structs. Like in the lecture, the idea is that we use scopes to get IDs from variable names, and all_variables to get values and the like from IDs. When you create a SymbolTable, all_variables is empty, but scope_stack is not: we push an empty scope to it. 

# Functions
PushScope just adds an empty scope to the top of the scope stack.

PopScope removes the topmost scope from the stack. It gives an error both if you try to pop an empty stack, and if you try to pop a stack with just one scope in it--that's the outermost, "global" scope, and we should never try to pop it. 

HasVar just checks whether there's a variable with the given name anywhere in the stack of scopes. 

AddVar is what we'll call when declaring a new variable. It creates a VariableInfo struct with the name and line number set to the given values, the variable value set to 0 (arbitrarily--as we'll see, we'll never attempt to get the value of a variable that we've declared without setting), and "initialized" set to false. If there's an entry in the topmost scope with the same name, it throws an error. Otherwise, it adds that VariableInfo to the all_variables vector, and puts an entry in the topmost scope with the index where the VariableInfo is stored.

GetValue looks through the scopes from top to bottom for a variable with the given name, and returns the value in the first scope that contains that name. If the name isn't in any of the scopes, it throws an error. It also throws an error if, in the first scope containing the given name, the variable has not been initialized, i.e. the code contains a declaration like "var x;" but hasn't yet included something like "x = 2". 

SetValue works just like GetValue, with the obvious difference that, once it's found the first scope containing the given name, it sets the value of that variable (stored in all_variables) to the given value. It also sets "initialized" to true for that variable. 

# How to Use This in the Project 
These are all the cases where we'd need to do something with the symbol table:

If we encounter a left curly bracket, we need to create a new scope. We do this by just calling PushScope. 

If we encounter a right curly bracket, we're done with the current scope, so we call PopScope. 

If we encounter a reference to an identifier, as part of an expression that needs to be evaluated, we call GetValue. 

For a "pure declaration" like "var x;", we call AddVar but not SetValue. 

For a "pure set statement" like "x = 2;", we call SetValue but not AddValue.

For a "simultaneous declaration and set statement" like "var x = 2;", we call AddVar, then SetValue. 