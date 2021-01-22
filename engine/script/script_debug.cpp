#include "script.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
static constexpr char lut[] = "0123456789abcdef";

/**
  The ‘org.gnu.gdb.riscv.cpu’ feature is required
  for RISC-V targets. It should contain the registers
  ‘x0’ through ‘x31’, and ‘pc’. Either the
  architectural names (‘x0’, ‘x1’, etc) can be used,
  or the ABI names (‘zero’, ‘ra’, etc).

  The ‘org.gnu.gdb.riscv.fpu’ feature is optional.
  If present, it should contain registers ‘f0’ through
  ‘f31’, ‘fflags’, ‘frm’, and ‘fcsr’. As with the cpu
  feature, either the architectural register names,
  or the ABI names can be used.

  The ‘org.gnu.gdb.riscv.virtual’ feature is optional.
  If present, it should contain registers that are not
  backed by real registers on the target, but are
  instead virtual, where the register value is
  derived from other target state. In many ways these
  are like GDBs pseudo-registers, except implemented
  by the target. Currently the only register expected
  in this set is the one byte ‘priv’ register that
  contains the target’s privilege level in the least
  significant two bits.

  The ‘org.gnu.gdb.riscv.csr’ feature is optional.
  If present, it should contain all of the targets
  standard CSRs. Standard CSRs are those defined in
  the RISC-V specification documents. There is some
  overlap between this feature and the fpu feature;
  the ‘fflags’, ‘frm’, and ‘fcsr’ registers could
  be in either feature. The expectation is that these
  registers will be in the fpu feature if the target
  has floating point hardware, but can be moved into
  the csr feature if the target has the floating
  point control registers, but no other floating
  point hardware.
**/
struct RSPClient;

struct RSP
{
	RSPClient* accept();
	int  fd() const noexcept { return server_fd; }

	RSP(Script&, uint16_t);
	~RSP();

private:
	Script& m_script;
	int server_fd;
};
struct RSPClient
{
	int current_exception() const;

	void reply_ack();
	void reply_ok();

	bool read();
	bool send(const char* str);
	bool sendf(const char* fmt, ...);
	void kill();

	RSPClient(Script& s, int fd);
	~RSPClient();

private:
	static const int PACKET_SIZE = 1000;
	int forge_packet(char* dst, size_t dstlen, const char*, int);
	int forge_packet(char* dst, size_t dstlen, const char*, va_list);
	void process_data();
	void handle_query();
	void handle_breakpoint();
	void handle_continue();
	void handle_step();
	void handle_executing();
	void handle_multithread();
	void handle_readmem();
	void handle_writemem();
	void report_gprs();
	void report_exception();
	Script& m_script;
	int sockfd;
	std::string buffer;
	Script::gaddr_t m_bp = 0;
};

RSP::RSP(Script& script, uint16_t port)
	: m_script{script}
{
	this->server_fd = socket(AF_INET, SOCK_STREAM, 0);

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
		&opt, sizeof(opt))) {
		close(server_fd);
		throw std::runtime_error("Failed to enable REUSEADDR/PORT");
	}
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	if (bind(server_fd, (struct sockaddr*) &address,
            sizeof(address)) < 0) {
		close(server_fd);
		throw std::runtime_error("GDB listener failed to bind to port");
	}
	if (listen(server_fd, 2) < 0) {
		close(server_fd);
		throw std::runtime_error("GDB listener failed to listen on port");
	}
}
RSPClient* RSP::accept()
{
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	int sockfd = ::accept(server_fd, (struct sockaddr*) &address,
        	(socklen_t*) &addrlen);
    if (sockfd < 0) {
		return nullptr;
	}
	return new RSPClient(m_script, sockfd);
}
RSP::~RSP()
{
	close(server_fd);
}

RSPClient::RSPClient(Script& script, int fd)
	: m_script{script}, sockfd(fd)
{
	while (this->read());
}
RSPClient::~RSPClient()
{
	close(this->sockfd);
}


int RSPClient::forge_packet(
	char* dst, size_t dstlen, const char* data, int datalen)
{
	char* d = dst;
	*d++ = '$';
	uint8_t csum = 0;
	for (int i = 0; i < datalen; i++) {
		uint8_t c = data[i];
		if (c == '$' || c == '#' || c == '*' || c == '}') {
			c ^= 0x20;
			csum += '}';
			*d++ = '}';
		}
		*d++ = c;
		csum += c;
	}
	*d++ = '#';
	*d++ = lut[(csum >> 4) & 0xF];
	*d++ = lut[(csum >> 0) & 0xF];
	return d - dst;
}
int RSPClient::forge_packet(
	char* dst, size_t dstlen, const char* fmt, va_list args)
{
	char data[4 + 2*PACKET_SIZE];
	int datalen = vsnprintf(data, sizeof(data), fmt, args);
	return forge_packet(dst, dstlen, data, datalen);
}
bool RSPClient::sendf(const char* fmt, ...)
{
	char buffer[PACKET_SIZE];
	va_list args;
	va_start(args, fmt);
	int plen = forge_packet(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	//printf(">>> %.*s\n", plen, buffer);
	int len = ::write(sockfd, buffer, plen);
	if (len <= 0) {
		return false;
	}
	// Acknowledgement
	int rlen = ::read(sockfd, buffer, 1);
	if (rlen <= 0) {
		return false;
	}
	return (buffer[0] == '+');
}
bool RSPClient::send(const char* str)
{
	char buffer[PACKET_SIZE];
	int plen = forge_packet(buffer, sizeof(buffer), str, strlen(str));
	//printf(">>> %.*s\n", plen, buffer);
	int len = ::write(sockfd, buffer, plen);
	if (len <= 0) {
		return false;
	}
	// Acknowledgement
	int rlen = ::read(sockfd, buffer, 1);
	if (rlen <= 0) {
		return false;
	}
	return (buffer[0] == '+');
}
bool RSPClient::read()
{
	char tmp[1024];
	int len = ::read(this->sockfd, tmp, sizeof(tmp));
	if (len <= 0) {
		return false;
	}
	//printf("READ: %.*s\n", len, tmp);
	for (int i = 0; i < len; i++)
	{
		char c = tmp[i];
		if (c == '$') {
			this->buffer.clear();
		}
		else if (c == '#') {
			this->process_data();
			this->buffer.clear();
			i += 2;
		}
		else {
			this->buffer.append(&c, 1);
			if (buffer.size() >= PACKET_SIZE)
				break;
		}
	}
	return true;
}
void RSPClient::process_data()
{
	reply_ack();
	switch (buffer[0]) {
	case 'q':
		handle_query();
		break;
	case 'c':
		handle_continue();
		break;
	case 's':
		handle_step();
		break;
	case 'g':
		report_gprs();
		break;
	case 'D':
	case 'k':
		kill();
		return;
	case 'H':
		handle_multithread();
		break;
	case 'm':
		handle_readmem();
		break;
	case 'v':
		handle_executing();
		break;
	case 'X':
		handle_writemem();
		break;
	case 'Z':
	case 'z':
		handle_breakpoint();
		break;
	case '?':
		report_exception();
		break;
	default:
		fprintf(stderr, "Unhandled packet: %c\n",
			buffer[0]);
	}
}
void RSPClient::handle_query()
{
	if (strncmp("qSupported", buffer.data(), strlen("qSupported")) == 0)
	{
		sendf("PacketSize=%x;swbreak-;hwbreak+", PACKET_SIZE);
	}
	else if (strncmp("qAttached", buffer.data(), strlen("qC")) == 0)
	{
		send("1");
	}
	else if (strncmp("qC", buffer.data(), strlen("qC")) == 0)
	{
		// Current thread ID
		send("QC0");
	}
	else if (strncmp("qOffsets", buffer.data(), strlen("qOffsets")) == 0)
	{
		// Section relocation offsets
		send("Text=0;Data=0;Bss=0");
	}
	else if (strncmp("qfThreadInfo", buffer.data(), strlen("qfThreadInfo")) == 0)
	{
		// Start of threads list
		send("m0");
	}
	else if (strncmp("qsThreadInfo", buffer.data(), strlen("qfThreadInfo")) == 0)
	{
		// End of threads list
		send("l");
	}
	else if (strncmp("qSymbol::", buffer.data(), strlen("qSymbol::")) == 0)
	{
		send("OK");
	}
	else if (strncmp("qTStatus", buffer.data(), strlen("qTStatus")) == 0)
	{
		send("");
	}
	else {
		fprintf(stderr, "Unknown query: %s\n",
			buffer.data());
		send("");
	}
}
void RSPClient::handle_continue()
{
	auto& machine = m_script.machine();
	try {
		machine.stop(false);
		while (!machine.stopped()) {
			machine.cpu.simulate();
			machine.increment_counter(1);
			// Breakpoint
			if (machine.cpu.pc() == this->m_bp)
				break;
		}
	} catch (...) {

	}
	send("S05");
}
void RSPClient::handle_step()
{
	auto& machine = m_script.machine();
	try {
		machine.cpu.simulate();
		machine.increment_counter(1);
	} catch (...) {

	}
	send("S05");
}
void RSPClient::handle_breakpoint()
{
	uint32_t type = 0;
	uint64_t addr = 0;
	sscanf(&buffer[1], "%x,%lx", &type, &addr);
	if (buffer[0] == 'Z') {
		this->m_bp = addr;
	} else {
		this->m_bp = 0;
	}
	reply_ok();
}
void RSPClient::handle_executing()
{
	if (strncmp("vRun", buffer.data(), strlen("vRun")) == 0)
	{
		printf("Signal: Run now\n");
	}
	else if (strncmp("vCont?", buffer.data(), strlen("vCont?")) == 0)
	{
		send("vContc;s");
	}
	else if (strncmp("vCont;c", buffer.data(), strlen("vCont;c")) == 0)
	{
		this->handle_continue();
	}
	else if (strncmp("vCont;s", buffer.data(), strlen("vCont;s")) == 0)
	{
		this->handle_step();
	}
	else if (strncmp("vMustReplyEmpty", buffer.data(), strlen("vMustReplyEmpty")) == 0)
	{
		send("");
	}
	else {
		fprintf(stderr, "Unknown executor: %s\n",
			buffer.data());
		send("");
	}
}
void RSPClient::handle_multithread()
{
	reply_ok();
}
void RSPClient::handle_readmem()
{
	uint64_t addr = 0;
	uint32_t len = 0;
	sscanf(buffer.c_str(), "m%lx,%x", &addr, &len);
	//printf("Addr: 0x%lX  Len: %u\n", addr, len);

	char data[1024];
	char* d = data;
	try {
		for (unsigned i = 0; i < len; i++) {
			uint8_t val =
			m_script.machine().memory.read<uint8_t> (addr + i);
			*d++ = lut[(val >> 4) & 0xF];
			*d++ = lut[(val >> 0) & 0xF];
		}
	} catch (...) {
		send("E01");
		return;
	}
	*d++ = 0;
	send(data);
}
void RSPClient::handle_writemem()
{
	uint64_t addr = 0;
	uint32_t len = 0;
	uint32_t val = 0;
	sscanf(buffer.c_str(), "X%lx,%x:%x", &addr, &len, &val);

	m_script.machine().memory.write<uint32_t> (addr, val);
}
void RSPClient::report_exception()
{
	sendf("S%02x", current_exception());
}

void RSPClient::report_gprs()
{
	auto& regs = m_script.machine().cpu.registers();
	char buffer[1024];
	char* d = buffer;
	/* GPRs */
	for (int i = 0; i < 32; i++) {
		auto reg = regs.get(i);
		for (int j = 0; j < sizeof(reg); j++) {
			*d++ = lut[(reg >> (j*8+4)) & 0xF];
			*d++ = lut[(reg >> (j*8+0)) & 0xF];
		}
	}
	/* PC */
	{
		auto reg = regs.pc;
		for (int j = 0; j < sizeof(reg); j++) {
			*d++ = lut[(reg >> (j*8+4)) & 0xF];
			*d++ = lut[(reg >> (j*8+0)) & 0xF];
		}
	}
	*d++ = 0;
	send(buffer);
}

void RSPClient::reply_ack()
{
	write(sockfd, "+", 1);
}
void RSPClient::reply_ok()
{
	send("OK");
}

int RSPClient::current_exception() const {
	return 0;
}

void RSPClient::kill()
{
	close(sockfd);
}

void Script::gdb_listen(uint16_t port)
{
	//machine().verbose_instructions = true;
	RSP server { *this, port };
	RSPClient* client = nullptr;
	try {
		client = server.accept();
	} catch (...) {
	}
	delete client;
}
