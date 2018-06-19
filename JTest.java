class JTest {

    private static String s;
    
    public static int __do(int a, int b, int c) {
	s = "" + a;
	return (a + b) % c;
    }
    
    private static int foo(int arg1, int arg2) {
	return __do(arg1, arg2, 123);
    }
    
    public static void main(String[] args) {
	int result = args.length;
	for (int i = 0; i < 100000; ++i) {
	    for (int ii = 0; ii < 1000; ++ii) {
		result = foo(result + 1, ii % args.hashCode());
	    }
	}
	System.out.println("s = " + s);
	System.out.println("result = " + result);
    }
}
