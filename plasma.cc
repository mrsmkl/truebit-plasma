
// Perhaps first try to get 256-bit values to work

#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
#include <stdio.h>
#include <map>
#include "keccak-tiny.h"
#include <secp256k1_recovery.h>

using u256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using s256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;  

u256 get_bytes32(FILE *f, bool &eof) {
    uint8_t *res = (uint8_t*)malloc(32);
    int ret = fread(res, 1, 32, f);
    printf("Got %i\n", ret);
    if (ret != 32) {
        printf("Error %i: %s\n", ferror(f), strerror(ferror(f)));
        free(res);
        eof = true;
        return 0;
    }
    u256 x;
    for (int i = 0; i < 32; i++) {
        x = x*256;
        x += res[i];
    }
    return x;
}

u256 get_bytes32(FILE *f) {
    bool foo;
    return get_bytes32(f, foo);
}

struct tr {
    u256 s, r;
    uint8_t v;
    u256 from;
    u256 to;
    u256 value;
};

struct Pending {
    u256 to;
    u256 value;
    u256 block; // block number
    Pending(u256 a, u256 b, u256 c) {
        to = a; value = b; block = c;
    }
    Pending() {
    }
    Pending(Pending const &p) {
        to = p.to;
        value = p.value;
        block = p.block;
    }
};

std::map<u256, u256> balances;
std::map<u256, u256> nonces;
std::map<u256, u256> block_hash;
std::map<u256, Pending> pending;

u256 block_number;

// well there are two modes, one is for transaction files, not all commands are allowed there

u256 max(u256 a, u256 b) {
    return a > b ? a : b;
}

std::vector<uint8_t> keccak256_v(std::vector<uint8_t> data) {
	// std::string out(32, 0);
	std::vector<uint8_t> out(32, 0);
	keccak::sha3_256(out.data(), 32, data.data(), data.size());
	return out;
}

std::vector<uint8_t> toBigEndian(u256 const &a) {
    u256 b = a;
    std::vector<uint8_t> res(32, 0);
    for (int i = res.size(); i != 0; i--) {
		res[i-1] = (uint8_t)b & 0xff;
        // b >>= 8;
        b = b / 256;
	}
    return res;
}

u256 fromBigEndian(std::vector<uint8_t> const &str) {
	u256 ret(0);
	for (auto i: str) ret = ((ret * 256) | (u256)i);
	return ret;
}

u256 fromBigEndian(std::vector<uint8_t>::iterator a, std::vector<uint8_t>::iterator b) {
	u256 ret(0);
    while (a != b) {
        ret = ((ret * 256) | (u256)*a);
        a++;
    }
	return ret;
}

u256 keccak256(std::vector<uint8_t> str) {
    return fromBigEndian(keccak256_v(str));
}

u256 keccak256(u256 a) {
    return keccak256(toBigEndian(a));
}

u256 keccak256(u256 a, u256 b) {
    std::vector<uint8_t> aa = toBigEndian(a);
    std::vector<uint8_t> bb = toBigEndian(b);
    aa.insert(std::end(aa), bb.begin(), bb.end());
    return keccak256(aa);
}

u256 keccak256(u256 a, u256 b, u256 c) {
    std::vector<uint8_t> aa = toBigEndian(a);
    std::vector<uint8_t> bb = toBigEndian(b);
    std::vector<uint8_t> cc = toBigEndian(c);
    aa.insert(std::end(aa), std::begin(bb), std::end(bb));
    aa.insert(std::end(aa), std::begin(cc), std::end(cc));
    return keccak256(aa);
}

u256 keccak256(u256 a, u256 b, u256 c, u256 d) {
    std::vector<uint8_t> aa = toBigEndian(a);
    std::vector<uint8_t> bb = toBigEndian(b);
    std::vector<uint8_t> cc = toBigEndian(c);
    std::vector<uint8_t> dd = toBigEndian(d);
    aa.insert(std::end(aa), std::begin(bb), std::end(bb));
    aa.insert(std::end(aa), std::begin(cc), std::end(cc));
    aa.insert(std::end(aa), std::begin(dd), std::end(dd));
    return keccak256(aa);
}

secp256k1_context const* getCtx() {
	static std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)> s_ctx{
		secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY),
		&secp256k1_context_destroy
	};
	return s_ctx.get();
}

u256 ecrecover(std::vector<uint8_t> const& _sig, std::vector<uint8_t> _message) {
	int v = _sig[64];
	if (v > 3) return 0;

	auto* ctx = getCtx();
	secp256k1_ecdsa_recoverable_signature rawSig;
	if (!secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rawSig, _sig.data(), v))
		return 0;

	secp256k1_pubkey rawPubkey;
	if (!secp256k1_ecdsa_recover(ctx, &rawPubkey, &rawSig, _message.data()))
		return 0;

	std::array<uint8_t, 65> serializedPubkey;
	size_t serializedPubkeySize = serializedPubkey.size();
	secp256k1_ec_pubkey_serialize(
			ctx, serializedPubkey.data(), &serializedPubkeySize,
			&rawPubkey, SECP256K1_EC_UNCOMPRESSED
	);
	assert(serializedPubkeySize == serializedPubkey.size());
	// Expect single byte header of value 0x04 -- uncompressed public key.
	assert(serializedPubkey[0] == 0x04);
	// Create the Public skipping the header.
    
    std::vector<uint8_t> out(32, 0);
	keccak::sha3_256(out.data(), 32, &serializedPubkey[1], 64);
	return fromBigEndian(out.begin()+12, out.end());
}

u256 ecrecover(u256 r, u256 s, u256 v, u256 hash) {
    std::vector<uint8_t> a = toBigEndian(r);
    std::vector<uint8_t> b = toBigEndian(s);
    std::vector<uint8_t> c = toBigEndian(v);
    a.insert(std::end(a), std::begin(b), std::end(b));
    a.insert(std::end(a), std::begin(c)+31, std::end(c));
    return ecrecover(a, toBigEndian(hash));
}

std::vector<u256> hashLevel(std::vector<u256> data) {
	std::vector<u256> res;
	res.resize(data.size() / 2);
	for (int i = 0; i < res.size(); i += 32) {
        res[i] = keccak256(data[i*2], data[i*2+1]);
	}
	return res;
}

u256 hashRec(std::vector<u256> res) {
    if (res.size() > 1) {
        return hashRec(hashLevel(res));
    }
    else return res[0];
}

u256 hashFile() {
    FILE *f = fopen("state.data", "rb");
    bool eof = false;
	std::vector<u256> res;
	res.resize(2);
    int level = 1;
    int i = 0;
    while (true) {
        u256 elem = get_bytes32(f, eof);
        if (eof) break;
        if (i == res.size()) {
            level++;
            res.resize(res.size()*2);
        }
        res[i] = elem;
    }
    fclose(f);
    
    return hashRec(res);
}

void process(FILE *f, u256 hash, bool restricted, bool &eof) {
    u256 control = get_bytes32(f);
    if (control == 0) {
        eof = true;
    }
    // Transaction: remove from account, add to pending
    if (control == 1) {
        u256 from = get_bytes32(f);
        u256 to = get_bytes32(f);
        u256 value = get_bytes32(f);
        u256 nonce = get_bytes32(f);
        u256 r = get_bytes32(f);
        u256 s = get_bytes32(f);
        u256 v = get_bytes32(f);
        u256 hash = keccak256(to, value, nonce);
        if (ecrecover(r, s, v, hash) != from) return;
        u256 bal = balances[from];
        if (bal < v || nonces[from] != nonce || pending.find(from) == pending.end()) return;
        balances[from] = bal - v;
        nonces[from]++;
        pending[from] = Pending(to, value, block_number);
    }
    // Confirm transaction
    else if (control == 2) {
        u256 from = get_bytes32(f);
        u256 hash = get_bytes32(f);
        u256 r = get_bytes32(f);
        u256 s = get_bytes32(f);
        u256 v = get_bytes32(f);
        if (ecrecover(r, s, v, hash) != from) return;
        Pending p = pending[from];
        if (block_hash[p.block] != hash) return;
        balances[p.to] = p.value;
        pending.erase(from);
    }
    if (restricted) return;
    // Block hash
    else if (control == 3) {
        u256 num = get_bytes32(f);
        u256 hash = get_bytes32(f);
        block_hash[num] = hash;
        block_number = max(num+1, block_number);
        block_hash[block_number] = hash;
    }
    // Balance, nonce
    else if (control == 4) {
        u256 addr = get_bytes32(f);
        u256 v = get_bytes32(f);
        u256 nonce = get_bytes32(f);
        balances[addr] = v;
        nonces[addr] = nonce;
    }
    // Pending transaction
    else if (control == 5) {
        u256 from = get_bytes32(f);
        u256 to = get_bytes32(f);
        u256 value = get_bytes32(f);
        u256 block = get_bytes32(f);
        pending[from] = Pending(to, value, block);
    }
}

void processFile(char const *fname, u256 hash, bool restr) {
    bool eof = false;
    FILE *f = fopen("state.data", "rb");
    while (!eof) {
        process(f, hash, restr, eof);
    }
    fclose(f);
}

void put_bytes32(FILE *f, u256 a) {
    std::vector<uint8_t> v = toBigEndian(a);
    fwrite(v.data(), 1, 32, f);
}

void finalize() {
    // open file for writing
    FILE *f = fopen("state.data", "rb");
    // output block hashes <-- old hashes could be removed
    for (auto const& x : block_hash) {
        put_bytes32(f, 3);
        put_bytes32(f, x.first);
        put_bytes32(f, x.second);
    }
    // output balances
    for (auto const& x : balances) {
        put_bytes32(f, 3);
        put_bytes32(f, x.first);
        put_bytes32(f, balances[x.first]);
        put_bytes32(f, nonces[x.first]);
    }
    // output pending
    for (auto const& x : pending) {
        put_bytes32(f, x.first);
        put_bytes32(f, x.second.to);
        put_bytes32(f, x.second.value);
        put_bytes32(f, x.second.block);
    }
    fclose(f);
}

// first thing is calculating the hash of the state

int main(int argc, char **argv) {
    u256 x;
    x++;
    std::cout << "Checking 256-bit values: " << x << std::endl;
    u256 hash = hashFile();
    processFile("state.data", hash, false);
    processFile("control.data", hash, false);
    processFile("input.data", hash, true);
    finalize();
    
    return 0;
}

