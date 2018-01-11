
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
       bytes32 tr;
       uint task;
       bytes32 state; // state is settled from the previous one
       bytes32 state_file;
    }

    Block[] blocks;

    function Plasma(address tb, address fs, bytes32 init_state, bytes32 init_file, string code_address, bytes32 code_hash) public {
       blocks.length = 1;
       blocks[0].state = init_state;
       blocks[0].state_file = init_file;
       truebit = TrueBit(tb);
       filesystem = Filesystem(fs);
       
       code = code_address;     // address for wasm file in IPFS
       init = code_hash;        // the canonical hash
    }

    function submitBlock(bytes32 b) public {
       uint bnum = blocks.length;
       blocks.length++;
       blocks[bnum].tr = b;
    }

    function validate(uint bnum, bytes32 file) public {
       Block storage b = blocks[bnum];
       require(b.tr == filesystem.getRoot(file));
       
       require(b.task == 0);
       Block storage last = blocks[bnum-1];
       bytes32 bundle = filesystem.makeBundle(bnum);
       filesystem.addToBundle(bundle, file);
       filesystem.addToBundle(bundle, last.state_file);
       filesystem.finalizeBundleIPFS(bundle, code, init);
      
       b.task = truebit.addWithParameters(filesystem.getInitHash(bundle), 1, 1, idToString(bundle), 20, 25, 8, 20, 10);
       truebit.requireFile(b.task, hashName("output.data"), 0);
       truebit.requireFile(b.task, hashName("state.data"), 1);

       task_to_id[b.task] = bnum;
    }

    function solved(uint id, bytes32[] files) public {
       // could check the task id
       uint bnum = task_to_id[id];
       Block storage b = blocks[bnum];
       
       b.state_file = files[0];
       b.state = filesystem.getRoot(files[0]);
       
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
}


