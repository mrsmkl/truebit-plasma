/*

Truebit task will be a program that gets as input two files, the system state (for example account balances) and the transactions
(not necessarily stored on blockchain).

Transactions in the child chain:
 * first, the value is substracted from the balance, transaction is pending
 * then the user has to commit to a block containing the new value (where transaxction is pending), after that the transaction is settled

*/

pragma solidity ^0.4.16;

interface Filesystem {

   function createFileWithContents(string name, uint nonce, bytes32[] arr, uint sz) public returns (bytes32);
   function getSize(bytes32 id) public view returns (uint);
   function getRoot(bytes32 id) public view returns (bytes32);
   function forwardData(bytes32 id, address a) public;
   
   // function makeSimpleBundle(uint num, address code, bytes32 code_init, bytes32 file_id) public returns (bytes32);
   
   function makeBundle(uint num) public view returns (bytes32);
   function addToBundle(bytes32 id, bytes32 file_id) public returns (bytes32);
   function finalizeBundleIPFS(bytes32 id, string file, bytes32 init) public;
   function getInitHash(bytes32 bid) public view returns (bytes32);
   
   // function debug_finalizeBundleIPFS(bytes32 id, string file, bytes32 init) public returns (bytes32, bytes32, bytes32, bytes32, bytes32);
   
}

interface TrueBit {
   function add(bytes32 init, /* CodeType */ uint8 ct, /* Storage */ uint8 cs, string stor) public returns (uint);
   function addWithParameters(bytes32 init, /* CodeType */ uint8 ct, /* Storage */ uint8 cs, string stor, uint8 stack, uint8 mem, uint8 globals, uint8 table, uint8 call) public returns (uint);
   function requireFile(uint id, bytes32 hash, /* Storage */ uint8 st) public;
}

contract Plasma {

    TrueBit truebit;
    Filesystem filesystem;
    
    mapping (uint => uint) task_to_id;
    
    string code;
    bytes32 init;

    struct Block {
       // there are two types of blocks
       bytes32 tr;       // 1
       bytes32[] input;  // 2
       
       uint stamp;
       
       uint task;
       bytes32 state; // state is settled from the previous one
       bytes32 state_file;
       bytes32 balance; // file with balances
       bytes32 balance_file;
    }

    Block[] blocks;

    function Plasma(address tb, address fs, bytes32 init_file, string code_address, bytes32 code_hash) public {
       truebit = TrueBit(tb);
       filesystem = Filesystem(fs);
       
       blocks.length = 1;
       blocks[0].state = filesystem.getRoot(init_file);
       blocks[0].state_file = init_file;
       
       code = code_address;     // address for wasm file in IPFS
       init = code_hash;        // the canonical hash
       
       pqueue.length = 1;       // Has only the sentinel
    }

    function submitBlock(bytes32 b) public {
       uint bnum = blocks.length;
       blocks.length++;
       blocks[bnum].tr = b;
       blocks[bnum].stamp = block.timestamp;
    }

    function deposit() public payable {
       uint bnum = blocks.length;
       blocks.length++;
       blocks[bnum].input.push(bytes32(4));
       blocks[bnum].input.push(bytes32(msg.sender));
       blocks[bnum].input.push(bytes32(msg.value));
       blocks[bnum].input.push(bytes32(0));
    }
    
    function debugBlock(uint num) public returns (bytes32[]) {
       return blocks[num].input;
    }

    uint nonce;

    function createEmptyFile(string fname) internal returns (bytes32) {
       nonce++;
       bytes32[] memory input = new bytes32[](2);
       return filesystem.createFileWithContents(fname, nonce+3000000, input, 0);
    }

    // need to check that the file has correct name
    function validate(uint bnum, bytes32 file) public {
       Block storage b = blocks[bnum];
       require(b.tr == filesystem.getRoot(file));
       
       require(b.task == 0);
       Block storage last = blocks[bnum-1];
       bytes32 bundle = filesystem.makeBundle(bnum);
       filesystem.addToBundle(bundle, file);
       filesystem.addToBundle(bundle, createEmptyFile("balances.data"));
       filesystem.addToBundle(bundle, createEmptyFile("control.data"));
       filesystem.addToBundle(bundle, last.state_file);
       filesystem.finalizeBundleIPFS(bundle, code, init);

       b.task = truebit.addWithParameters(filesystem.getInitHash(bundle), 1, 1, idToString(bundle), 20, 25, 8, 20, 10);
       truebit.requireFile(b.task, hashName("state.data"), 1);
       truebit.requireFile(b.task, hashName("balances.data"), 1);

       task_to_id[b.task] = bnum;
    }

    // need to check that the file has correct name
    function validateDeposit(uint bnum) public {
       Block storage b = blocks[bnum];
       
       require(b.task == 0);
       Block storage last = blocks[bnum-1];
       bytes32 bundle = filesystem.makeBundle(bnum);
       bytes32 file = filesystem.createFileWithContents("control.data", bnum, b.input, b.input.length*32);
       filesystem.addToBundle(bundle, file);
       filesystem.addToBundle(bundle, last.state_file);
       filesystem.addToBundle(bundle, createEmptyFile("balances.data"));
       filesystem.addToBundle(bundle, createEmptyFile("input.data"));
       filesystem.finalizeBundleIPFS(bundle, code, init);

       b.task = truebit.addWithParameters(filesystem.getInitHash(bundle), 1, 1, idToString(bundle), 20, 25, 8, 20, 10);
       truebit.requireFile(b.task, hashName("state.data"), 1);
       truebit.requireFile(b.task, hashName("balances.data"), 1);

       task_to_id[b.task] = bnum;
    }

    function solved(uint id, bytes32[] files) public {
       // could check the task id
       uint bnum = task_to_id[id];
       Block storage b = blocks[bnum];
       
       b.state_file = files[0];
       b.state = filesystem.getRoot(files[0]);
       
       b.balance_file = files[1];
       b.balance = filesystem.getRoot(files[1]);
       
    }
   
   // Perhaps the state could have address / value pairs
   // How can a transaction be finalized in under a week?
   
   // Processing exits?
   // From priority queue, can process old enough messages
   
   struct Elem {
      uint next;
      uint prev;
      
      // the proof can be checked already when submitting
      uint block;
      address addr;
      uint value;
      uint time; // wait for 
   }
   
   Elem[] pqueue;
   
   function progress() public {
      Elem storage x = pqueue[pqueue[0].next];
      require(blocks[x.block].stamp + 7 days < block.timestamp);
      x.addr.transfer(x.value);
      pqueue[0].next = x.next;
      pqueue[x.next].prev = 0;
      // refund gas
      x.next = 0; x.prev = 0; x.block = 0; x.addr = 0; x.value = 0;
   }
   
   function startExit(uint bnum, uint pos, bytes32[] proof) public {
       require(bytes32(msg.sender) == proof[0]);
       Block storage b = blocks[bnum];
       
       require(blocks[bnum].stamp + 7 days > block.timestamp);
       require(b.balance == getRoot(proof, pos));
       
       uint e_pos = pqueue.length;
       pqueue.length++;
       Elem storage elem = pqueue[e_pos];
       
       elem.block = bnum;
       elem.addr = msg.sender;
       elem.value = uint(proof[1]);
       elem.time = block.timestamp;
   }
   
   function challengeExit(uint e_pos, bytes32 proof, uint8 v, bytes32 r, bytes32 s, uint bnum) public {
       Elem storage elem = pqueue[e_pos];
       
       require(bnum > elem.block);
       require(keccak256("Truebit Plasma", this, blocks[bnum].state) == proof);
       require(ecrecover(proof, v, r, s) == elem.addr);
       
       elem.time = uint(-1);
   }
   
   function exitTimeout(uint e_pos, uint pqhint) public {
   
       Elem storage elem = pqueue[e_pos];
       
       require(elem.time + 1 days < block.timestamp);

       Elem storage prev = pqueue[pqhint];
       Elem storage next = pqueue[prev.next];
       
       require(prev.addr == 0 || prev.block <= elem.block);
       require(next.addr == 0 || next.block > elem.block);
       
       elem.next = prev.next;
       elem.prev = next.prev;
       
       prev.next = e_pos;
       next.prev = e_pos;
   }

   ////////////////////////////////////
    
   function idToString(bytes32 id) public pure returns (string) {
      bytes memory res = new bytes(64);
      for (uint i = 0; i < 64; i++) res[i] = bytes1(((uint(id) / (2**(4*i))) & 0xf) + 65);
      return string(res);
   }

   function makeMerkle(bytes arr, uint idx, uint level) internal pure returns (bytes32) {
      if (level == 0) return idx < arr.length ? bytes32(uint(arr[idx])) : bytes32(0);
      else return keccak256(makeMerkle(arr, idx, level-1), makeMerkle(arr, idx+(2**(level-1)), level-1));
   }

   function calcMerkle(bytes32[] arr, uint idx, uint level) internal returns (bytes32) {
      if (level == 0) return idx < arr.length ? arr[idx] : bytes32(0);
      else return keccak256(calcMerkle(arr, idx, level-1), calcMerkle(arr, idx+(2**(level-1)), level-1));
   }

   // assume 256 bytes?
   function hashName(string name) public pure returns (bytes32) {
      return makeMerkle(bytes(name), 0, 8);
   }
   
   function getRoot(bytes32[] proof, uint loc) internal pure returns (bytes32) {
        require(proof.length >= 2);
        bytes32 res = keccak256(proof[0], proof[1]);
        for (uint i = 2; i < proof.length; i++) {
            loc = loc/2;
            if (loc%2 == 0) res = keccak256(res, proof[i]);
            else res = keccak256(proof[i], res);
        }
        require(loc < 2);
        return res;
   }
   
}


