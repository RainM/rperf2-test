import java.util.ArrayList;

class JTest {

    private static String s;
    private static ArrayList<String> lst = new ArrayList();

    
    public static int __do(int a, int b, int c) {
	s = "" + a;
	lst.add(" -> " + s);
	return (a + b) % (c + lst.size());
    }
    
    private static int foo(int arg1, int arg2) {
	return __do(arg1, arg2, 123);
    }
    
    public static void main(String[] args) {
	int result = args.length;
	for (int i = 0; i < 10000; ++i) {
	    for (int ii = 0; ii < 1000; ++ii) {
		result = foo(result + 1, ii % args.hashCode());
	    }
	}
	System.out.println("s = " + s);
	System.out.println("result = " + result);
    }
}
