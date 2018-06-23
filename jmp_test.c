int foo(int (*h)()) {
  asm("jmp 0x400f6e");
  return h();
}

int main() {

}
