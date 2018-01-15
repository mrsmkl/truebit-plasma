
// Perhaps first try to get 256-bit values to work

#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
#include <stdio.h>
#include <map>
    
using u256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using s256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;  

u256 get_bytes32(FILE *f) {
    uint8_t *res = (uint8_t*)malloc(32);
    int ret = fread(res, 1, 32, f);
    printf("Got %i\n", ret);
    if (ret != 32) {
        printf("Error %i: %s\n", ferror(f), strerror(ferror(f)));
        free(res);
        return 0;
    }
    u256 x;
    for (int i = 0; i < 32; i++) {
        x = x*256;
        x += res[i];
    }
    return x;
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

void finalize() {
    // open file for writing
    FILE *f = fopen("state.data", "rb");
    // output balances
    // output pending
    // output block hashes <-- old hashes could be removed
    // also hash that was calculated for the state that was read
}

// well there are two modes, one is for transaction files, not all commands are allowed there

u256 max(u256 a, u256 b) {
    return a > b ? a : b;
}

void process(FILE *f, u256 hash) {
    u256 control = get_bytes32(f);
    if (control == 0) {
        fclose(f);
        finalize();
        exit(0);
    }
    // Balance, nonce
    else if (control == 1) {
        u256 addr = get_bytes32(f);
        u256 v = get_bytes32(f);
        u256 nonce = get_bytes32(f);
        balances[addr] = v;
        nonces[addr] = nonce;
    }
    // Pending transaction
    else if (control == 2) {
        u256 from = get_bytes32(f);
        u256 to = get_bytes32(f);
        u256 value = get_bytes32(f);
        u256 block = get_bytes32(f);
        pending[from] = Pending(to, value, block);
    }
    // Block hash
    else if (control == 3) {
        u256 num = get_bytes32(f);
        u256 hash = get_bytes32(f);
        block_hash[num] = hash;
        block_number = max(num+1, block_number);
    }
    // Transaction: remove from account, add to pending
    else if (control == 4) {
        u256 from = get_bytes32(f);
        u256 to = get_bytes32(f);
        u256 value = get_bytes32(f);
        u256 nonce = get_bytes32(f);
        u256 r = get_bytes32(f);
        u256 s = get_bytes32(f);
        u256 v = get_bytes32(f);
        // TODO: Check signature
        u256 bal = balances[from];
        if (bal < v || nonces[from] != nonce || pending.find(from) == pending.end()) return;
        balances[from] = bal - v;
        nonces[from]++;
        pending[from] = Pending(to, value, block_number);
    }
    // Confirm transaction
    else if (control == 5) {
        u256 from = get_bytes32(f);
        u256 hash = get_bytes32(f);
        Pending p = pending[from];
        
    }
}

// first thing is calculating the hash of the state

int main(int argc, char **argv) {
    u256 x;
    x++;
    std::cout << "Checking 256-bit values: " << x << std::endl;
    /*
    FILE *f = fopen("state.data", "rb");
    process(f);
    */
    return 0;
}

