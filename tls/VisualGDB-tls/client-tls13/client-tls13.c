#include <stdio.h>

int main(int argc, char *argv[])
{
	char sz[] = "Hello, World!\n";	/* Hover mouse over "sz" while debugging to see its contents */
<<<<<<< HEAD
	printf("%s", sz);	
=======
	printf("%s", sz);
>>>>>>> 3661692c1325fc5fff97c81268f1666462fb771a
	fflush(stdout); /* <============== Put a breakpoint here */
	return 0;
}