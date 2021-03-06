/*
Based on:
100% free public domain implementation of the SHA-1 algorithm
by Dominik Reichl <Dominik.Reichl@tiscali.de>

Refactored in C++ style as part of openMSX
by Maarten ter Huurne and Wouter Vermaelen.

=== Test Vectors (from FIPS PUB 180-1) ===

"abc"
A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D

"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1

A million repetitions of "a"
34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/

#include "sha1.hh"
#include "MSXException.hh"
#include "CliComm.hh"
#include "EventDistributor.hh"
#include "StringOp.hh"
#include "endian.hh"
#include <cassert>
#include <cstring>

using std::string;

namespace openmsx {

// Rotate x bits to the left
inline static uint32_t rol32(uint32_t value, int bits)
{
	return (value << bits) | (value >> (32 - bits));
}

class WorkspaceBlock {
private:
	uint32_t data[16];

	uint32_t next0(int i)
	{
		data[i] = Endian::readB32(&data[i]);
		return data[i];
	}
	uint32_t next(int i)
	{
		return data[i & 15] = rol32(
			data[(i + 13) & 15] ^ data[(i + 8) & 15] ^
			data[(i +  2) & 15] ^ data[ i      & 15]
			, 1);
	}

public:
	explicit WorkspaceBlock(const uint8_t buffer[64]);

	// SHA-1 rounds
	void r0(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += ((w & (x ^ y)) ^ y) + next0(i) + 0x5A827999 + rol32(v, 5);
		w = rol32(w, 30);
	}
	void r1(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += ((w & (x ^ y)) ^ y) + next(i) + 0x5A827999 + rol32(v, 5);
		w = rol32(w, 30);
	}
	void r2(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += (w ^ x ^ y) + next(i) + 0x6ED9EBA1 + rol32(v, 5);
		w = rol32(w, 30);
	}
	void r3(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += (((w | x) & y) | (w & x)) + next(i) + 0x8F1BBCDC + rol32(v, 5);
		w = rol32(w, 30);
	}
	void r4(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += (w ^ x ^ y) + next(i) + 0xCA62C1D6 + rol32(v, 5);
		w = rol32(w, 30);
	}
};

WorkspaceBlock::WorkspaceBlock(const uint8_t buffer[64])
{
	memcpy(data, buffer, sizeof(data));
}


// class Sha1Sum

Sha1Sum::Sha1Sum()
{
	clear();
}

Sha1Sum::Sha1Sum(string_ref str)
{
	if (str.size() != 40) {
		throw MSXException("Invalid sha1, should be exactly 40 digits long: " + str);
	}
	parse40(str.data());
}

static inline unsigned hex(char x, const char* str)
{
	if (('0' <= x) && (x <= '9')) return x - '0';
	if (('a' <= x) && (x <= 'f')) return x - 'a' + 10;
	if (('A' <= x) && (x <= 'F')) return x - 'A' + 10;
	throw MSXException("Invalid sha1, digits should be 0-9, a-f: " +
	                   string(str, 40));
}
void Sha1Sum::parse40(const char* str)
{
	const char* p = str;
	for (int i = 0; i < 5; ++i) {
		unsigned t = 0;
		for (int j = 0; j < 8; ++j) {
			t <<= 4;
			t |= hex(*p++, str);
		}
		a[i] = t;
	}
}

static inline char digit(unsigned x)
{
	return (x < 10) ? (x + '0') : (x - 10 + 'a');
}
std::string Sha1Sum::toString() const
{
	char buf[40];
	char* p = buf;
	for (int i = 0; i < 5; ++i) {
		for (int j = 28; j >= 0; j -= 4) {
			*p++ = digit((a[i] >> j) & 0xf);
		}
	}
	return string(buf, 40);
}

bool Sha1Sum::empty() const
{
	for (int i = 0; i < 5; ++i) {
		if (a[i] != 0) return false;
	}
	return true;
}
void Sha1Sum::clear()
{
	for (int i = 0; i < 5; ++i) {
		a[i] = 0;
	}
}

bool Sha1Sum::operator==(const Sha1Sum& other) const
{
	for (int i = 0; i < 5; ++i) {
		if (a[i] != other.a[i]) return false;
	}
	return true;
}
bool Sha1Sum::operator!=(const Sha1Sum& other) const
{
	return !(*this == other);
}

bool Sha1Sum::operator<(const Sha1Sum& other) const
{
	for (int i = 0; i < 5-1; ++i) {
		if (a[i] != other.a[i]) return a[i] < other.a[i];
	}
	return a[5-1] < other.a[5-1];
}


// class SHA1

SHA1::SHA1()
{
	// SHA1 initialization constants
	m_state.a[0] = 0x67452301;
	m_state.a[1] = 0xEFCDAB89;
	m_state.a[2] = 0x98BADCFE;
	m_state.a[3] = 0x10325476;
	m_state.a[4] = 0xC3D2E1F0;

	m_count = 0;
	m_finalized = false;
}

void SHA1::transform(const uint8_t buffer[64])
{
	WorkspaceBlock block(buffer);

	// Copy m_state[] to working vars
	uint32_t a = m_state.a[0];
	uint32_t b = m_state.a[1];
	uint32_t c = m_state.a[2];
	uint32_t d = m_state.a[3];
	uint32_t e = m_state.a[4];

	// 4 rounds of 20 operations each. Loop unrolled
	block.r0(a,b,c,d,e, 0); block.r0(e,a,b,c,d, 1); block.r0(d,e,a,b,c, 2);
	block.r0(c,d,e,a,b, 3); block.r0(b,c,d,e,a, 4); block.r0(a,b,c,d,e, 5);
	block.r0(e,a,b,c,d, 6); block.r0(d,e,a,b,c, 7); block.r0(c,d,e,a,b, 8);
	block.r0(b,c,d,e,a, 9); block.r0(a,b,c,d,e,10); block.r0(e,a,b,c,d,11);
	block.r0(d,e,a,b,c,12); block.r0(c,d,e,a,b,13); block.r0(b,c,d,e,a,14);
	block.r0(a,b,c,d,e,15); block.r1(e,a,b,c,d,16); block.r1(d,e,a,b,c,17);
	block.r1(c,d,e,a,b,18); block.r1(b,c,d,e,a,19); block.r2(a,b,c,d,e,20);
	block.r2(e,a,b,c,d,21); block.r2(d,e,a,b,c,22); block.r2(c,d,e,a,b,23);
	block.r2(b,c,d,e,a,24); block.r2(a,b,c,d,e,25); block.r2(e,a,b,c,d,26);
	block.r2(d,e,a,b,c,27); block.r2(c,d,e,a,b,28); block.r2(b,c,d,e,a,29);
	block.r2(a,b,c,d,e,30); block.r2(e,a,b,c,d,31); block.r2(d,e,a,b,c,32);
	block.r2(c,d,e,a,b,33); block.r2(b,c,d,e,a,34); block.r2(a,b,c,d,e,35);
	block.r2(e,a,b,c,d,36); block.r2(d,e,a,b,c,37); block.r2(c,d,e,a,b,38);
	block.r2(b,c,d,e,a,39); block.r3(a,b,c,d,e,40); block.r3(e,a,b,c,d,41);
	block.r3(d,e,a,b,c,42); block.r3(c,d,e,a,b,43); block.r3(b,c,d,e,a,44);
	block.r3(a,b,c,d,e,45); block.r3(e,a,b,c,d,46); block.r3(d,e,a,b,c,47);
	block.r3(c,d,e,a,b,48); block.r3(b,c,d,e,a,49); block.r3(a,b,c,d,e,50);
	block.r3(e,a,b,c,d,51); block.r3(d,e,a,b,c,52); block.r3(c,d,e,a,b,53);
	block.r3(b,c,d,e,a,54); block.r3(a,b,c,d,e,55); block.r3(e,a,b,c,d,56);
	block.r3(d,e,a,b,c,57); block.r3(c,d,e,a,b,58); block.r3(b,c,d,e,a,59);
	block.r4(a,b,c,d,e,60); block.r4(e,a,b,c,d,61); block.r4(d,e,a,b,c,62);
	block.r4(c,d,e,a,b,63); block.r4(b,c,d,e,a,64); block.r4(a,b,c,d,e,65);
	block.r4(e,a,b,c,d,66); block.r4(d,e,a,b,c,67); block.r4(c,d,e,a,b,68);
	block.r4(b,c,d,e,a,69); block.r4(a,b,c,d,e,70); block.r4(e,a,b,c,d,71);
	block.r4(d,e,a,b,c,72); block.r4(c,d,e,a,b,73); block.r4(b,c,d,e,a,74);
	block.r4(a,b,c,d,e,75); block.r4(e,a,b,c,d,76); block.r4(d,e,a,b,c,77);
	block.r4(c,d,e,a,b,78); block.r4(b,c,d,e,a,79);

	// Add the working vars back into m_state[]
	m_state.a[0] += a;
	m_state.a[1] += b;
	m_state.a[2] += c;
	m_state.a[3] += d;
	m_state.a[4] += e;
}

// Use this function to hash in binary data and strings
void SHA1::update(const uint8_t* data, size_t len)
{
	assert(!m_finalized);
	uint32_t j = (m_count >> 3) & 63;

	m_count += uint64_t(len) << 3;

	uint32_t i;
	if ((j + len) > 63) {
		memcpy(&m_buffer[j], data, (i = 64 - j));
		transform(m_buffer);
		for (; i + 63 < len; i += 64) {
			transform(&data[i]);
		}
		j = 0;
	} else {
		i = 0;
	}
	memcpy(&m_buffer[j], &data[i], len - i);
}

void SHA1::finalize()
{
	assert(!m_finalized);
	uint8_t finalcount[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	for (int i = 0; i < 8; i++) {
		finalcount[i] = uint8_t(m_count >> ((7 - i) * 8));
	}

	update(reinterpret_cast<const uint8_t*>("\200"), 1);
	while ((m_count & 504) != 448) {
		update(reinterpret_cast<const uint8_t*>("\0"), 1);
	}
	update(finalcount, 8); // cause a transform()
	m_finalized = true;
}

Sha1Sum SHA1::digest()
{
	if (!m_finalized) finalize();
	return m_state;
}

Sha1Sum SHA1::calc(const uint8_t* data, size_t len)
{
	SHA1 sha1;
	sha1.update(data, len);
	return sha1.digest();
}

static void reportProgress(const string& filename, size_t percentage, CliComm& cliComm, EventDistributor& distributor) {
	cliComm.printProgress("Calculating SHA1 sum for " + filename + "... " + StringOp::toString(percentage) + "%");
	distributor.deliverEvents();
}

Sha1Sum SHA1::calcWithProgress(const uint8_t* data, size_t len, const string& filename, CliComm& cliComm, EventDistributor& distributor)
{
	if (len < 10*1024*1024) { // for small files, don't show progress
		return calc(data, len);
	}
	SHA1 sha1;
	static const size_t NUMBER_OF_STEPS = 100;
	// calculate in NUMBER_OF_STEPS steps and report progress every step
	auto stepSize = len / NUMBER_OF_STEPS;
	auto remainder = len % NUMBER_OF_STEPS;
	size_t offset = 0;
	reportProgress(filename, 0, cliComm, distributor);
	for (size_t i = 0; i < (NUMBER_OF_STEPS - 1); ++i) {
		sha1.update(&data[offset], stepSize);
		offset += stepSize;
		reportProgress(filename, i + 1, cliComm, distributor);
	}
	sha1.update(data + offset, stepSize + remainder);
	reportProgress(filename, 100, cliComm, distributor);
	return sha1.digest();
}

} // namespace openmsx
