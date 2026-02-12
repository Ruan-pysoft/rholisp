def fac(n):
    acc = 1
    while n:
        acc *= n
        n -= 1
    return acc

def fib(n):
    a = 0
    b = 1
    while n:
        a, b = b, a+b
        n -= 1
    return a

def triag(n):
    acc = 0
    while n:
        acc += n
        n -= 1
    return acc

print(f"{5 * 4=}")
print(f"{fac(5)=}")
print(f"{fac(10)=}")

for i in range(16):
    print(f"fib({i}) = {fib(i)}")
for i in range(16):
    print(f"fac({i}) = {fac(i)}")
for i in range(16):
    print(f"triag({i}) = {triag(i)}")

def fold(init, fn, lst):
    res = init
    for e in lst:
        res = fn(res, e)
    return res

lst = list(range(16))
print(f"{lst=}")
print(f"{fold(0, int.__add__, lst)=}")
print(f"{fold(1, int.__mul__, lst[1:])=}")

print(f"{2**14=}")
print(f"{3**10=}")

def fib_slow(n):
    if n < 2:
        return n
    else:
        return fib_slow(n-1) + fib_slow(n-2)

print(f"{fib_slow=}")
print(f"{fib_slow((30,20)[-1])=}")
print(f"{fib_slow(10)=}")
print(f"{fib_slow(15)=}")

def collatz(n):
    while n != 1:
        print(n)
        if n&1:
            n = 3*n + 1
        else:
            n = n//2
    print(n)

for i in range(1, 32):
    print(f"collatz({i}):")
    collatz(i)

assert 2**10 == 1024
assert 3**3 == 25 and "this should fail"
