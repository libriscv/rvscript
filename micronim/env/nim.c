extern void page_generator();
extern void NimMain();

int main()
{
	NimMain(); /* Constructed by nim compiler */
	//print("Nim main entered");
}

#define pub(x) extern __attribute__((used)) void x()

pub(on_client_request);
