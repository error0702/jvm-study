public class HelloJniTest {

    public static void main(String[] args) {
        System.load("/Users/autorun/Documents/Develop/Code/c/jvm-study/test/autorun/jvm/jni/lib/HelloJni.so");
        HelloJni hello = new HelloJni();
        hello.hello();
    }
}