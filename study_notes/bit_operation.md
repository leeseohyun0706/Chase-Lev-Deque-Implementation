# Bitwise Operation and for its Optimization in C++

As a computer load and store any value either `0` or `1`, called `bit` or `binary`, 
a bitwise operation calculates and manipulates on a bit-array (byte) at the level of each bit.
In particular, the bitwise operation contributes to a low-level system in which are required substantially faster performances.

## Bitwise Operators

### Not (~)
The NOT operation changes the current bits into its complement.
If the bit is `1`, the result will be then `0`. As the same way, if the bit is `0`, the result will be `1`.

```c++
uint8_t a = 0b11001100;                             // 1100 1100 (204)
std::cout << std::bitset<8>(~a) << std::endl;    // 0011 0011 (51)
```

### And (&)
The AND operation compares individual bits at the same position of another term for checking whether corresponding bits are the same.
If both bits are 1, the bit representing the result bit is `1` (`True AND True`). 
Otherwise, the result is `0` (`False AND (True OR False)`)

```c++
uint8_t a = 0b01100111;                             // 0110 0111 (103)
uint8_t b = 0b10101011;                             // 1010 1011 (171)
std::cout << std::bitset<8>(a & b) << std::endl;    // 0010 0011 (35)
```

| a | b | Boolean           | a & b |
|---|---|-------------------|-------|
| 0 | 1 | `False AND True`  | 0     |
| 1 | 0 | `True AND False`  | 0     |
| 1 | 1 | `True AND True`   | 1     |
| 0 | 0 | `False AND False` | 0     |
| 0 | 1 | `False AND True`  | 0     |
| 1 | 0 | `True AND False`  | 0     |
| 1 | 1 | `True AND True`   | 1     |
| 1 | 1 | `True AND True`   | 1     |

### Or (|)
The OR operation compares individual bits at the same position, and the result bit is `1` if at least one of the corresponding bits is `1`.

```c++
uint8_t a = 0b01100111;                             // 0110 0111 (103)
uint8_t b = 0b10101011;                             // 1010 1011 (171)
std::cout << std::bitset<8>(a | b) << std::endl;    // 1110 1111 (119)
```
| a | b | Boolean          | a & b |
|---|---|------------------|-------|
| 0 | 1 | `False OR True`  | 1     |
| 1 | 0 | `True OR False`  | 1     |
| 1 | 1 | `True OR True`   | 1     |
| 0 | 0 | `False OR False` | 0     |
| 0 | 1 | `False OR True`  | 1     |
| 1 | 0 | `True OR False`  | 1     |
| 1 | 1 | `True OR True`   | 1     |
| 1 | 1 | `True OR True`   | 1     |

### Xor (^)

### Left-Shift (<<)

The Left-Shift operation shifts each bit to the left by a specific amount of positions.
```c++
uint8_t a = 0b00010011; // 0001 0011 (19)
std::cout << std::bitset<8>(a << 2) << std::endl; // 0100 1100 (76)
uint8_t b = 0b01101100; // 0110 1100 (108)
std::cout << std::bitset<8>(b << 2) << std::endl; // 1011 0000 (176) (Bits out of the bitset are eliminated).
```

### Right-Shift (>>)
```c++
uint8_t a = 0b00010011; // 0001 0011
std::cout << std::bitset<8>(a >> 2) << std::endl; // 0000 0100 
uint8_t b = 0b01101100; // 0110 1100
std::cout << std::bitset<8>(b >> 2) << std::endl; // 0001 1011
```
## Optimization Technique
### Verify for Power of 2

In CPU level, the base-2 bits representing the power of 2 in integers (base-10) have special patterns in their bits.
For example, supposes a set of 8(n) bits `[1000 0000]`, and expresses it as a base-10 representation which results `128` (`2^(8-1)`). 
When we express `128 - 1` as the base-2 bits, it will be `[0111 1111]`. 
Then, operates these two bits with AND operation, and the result will be `[0000 0000]`, which is just `0`.

[//]: # (Similarly, when a set of 4&#40;n&#41; bits `[1000]`, it is expressed as a base-10 number `8` &#40;`2^&#40;4-1&#41;`&#41;.)

By using those features, we can verify an integer to be a power of 2.
The below function reads one argument as `int` and check for power of 2 using bitwise operations.
```c++
bool is_power_of_two(int n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}
```
We can test it by the following code,
```c++
std::cout << is_power_of_two(32) << std::endl; // [1000] & [0111] => [0000] == 0 : True
std::cout << is_power_of_two(36) << std::endl; // [10 0100] & [10 0011] => [10 0000] =! 0 : False
std::cout << is_power_of_two(1024) << std::endl; // [100 0000 0000] & [011 1111 1111] => [000 0000 0000] == 0 : True
std::cout << is_power_of_two(27) << std::endl;
```

### The Matter of Checking for Power of 2
We introduced the way to verify such a number whether a power of 2.
This idiom provides for C++ programs a faster speed of indexing process.

In general cases, programmers use the modular operation `%` to implement rotating an array or circular queue.
For instance, supposes that we try to make the Circular Queue which is rotating its front and rear positions by dequeue and enqueue.
In order to prevent out-of-range errors for its internal buffer, the memory access for the buffer should be limited to the absolute capacity of the buffer.

```c++
size_t capacity = 10;
int[] queue = new int[capacity];
int front = 0;
int rear = -1;
bool enqueue(int x) {
    ...
    queue[rear] = x;
    rear = (rear + 1) % capacity; // x % capacity limits to its value range on [0, capacity) 
    ...
}
bool dequeue(int x) {
    ...
    front = (front + 1) % capacity;
    ...
}
...
```




References

https://www.geeksforgeeks.org/c/bitwise-operators-in-c-cpp/
https://www.geeksforgeeks.org/cpp/cpp-bitwise-operators/
https://en.wikipedia.org/wiki/Bitwise_operation
https://dev.to/craftedwithintent/demystifying-algorithms-modular-indexing-3b5b
