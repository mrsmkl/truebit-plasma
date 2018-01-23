
var fs = require("fs")
var Web3 = require('web3')
var web3 = new Web3()

var wasm_path = "/home/sami/ocaml-offchain/interpreter/wasm"

var dir = "./compiled/"

var code = "0x" + fs.readFileSync(dir + "Plasma.bin")
var abi = JSON.parse(fs.readFileSync(dir + "Plasma.abi"))

var config = JSON.parse(fs.readFileSync("/home/sami/webasm-solidity/node/config.json"))

var host = config.host

var send_opt = {gas:4700000, from:config.base}

var w3provider = new web3.providers.WebsocketProvider('ws://' + host + ':8546')
web3.setProvider(w3provider)

var filesystem = new web3.eth.Contract(JSON.parse(fs.readFileSync("/home/sami/webasm-solidity/contracts/compiled/Filesystem.abi")), config.fs)

function arrange(arr) {
    var res = []
    var acc = ""
    arr.forEach(function (b) { acc += b; if (acc.length == 64) { res.push("0x"+acc); acc = "" } })
    if (acc != "") res.push("0x"+acc)
    console.log(res)
    return res
}

async function createFile(fname, buf) {
    var nonce = await web3.eth.getTransactionCount(config.base)
    var arr = []
    for (var i = 0; i < buf.length; i++) {
        if (buf[i] > 15) arr.push(buf[i].toString(16))
        else arr.push("0" + buf[i].toString(16))
    }
    console.log("Nonce", nonce, {arr:arrange(arr)})
    var tx = await filesystem.methods.createFileWithContents(fname, nonce, arrange(arr), buf.length).send(send_opt)
    var id = await filesystem.methods.calcId(nonce).call(send_opt)
    return id
}

function uploadIPFS(fname) {
    return new Promise(function (cont,err) {
        fs.readFile(fname, function (err, buf) {
            ipfs.files.add([{content:buf, path:fname}], function (err, res) {
                cont(res[0])
            })
        })
    })
}

function exec(cmd, lst) {
    return new Promise(function (cont,err) {
        execFile(cmd, args, function (error, stdout, stderr) {
            if (stderr) logger.error('error %s', stderr, args)
            if (stdout) logger.info('output %s', stdout, args)
            if (error) err(error)
            else cont(stdout)
        })
    })
}

async function createIPFSFile(config, fname, new_name) {
    new_name = new_name || fname
    var hash = await uploadIPFS(fname)
    var info = JSON.parse(await exec(wasm_path, ["-hash-file", fname]))
    var nonce = await web3.eth.getTransactionCount(base)
    console.log("Adding ipfs file", {name:new_name, size:info.size, ipfs_hash:hash.hash, data:info.root, nonce:nonce})
    var tx = await filesystem.methods.addIPFSFile(new_name, info.size, hash.hash, info.root, nonce).send(send_opt)
    var id = await filesystem.methods.calcId(nonce).call(send_opt)
    return id
}

async function outputFile(id) {
    var lst = await filesystem.methods.getRoot(id).call(send_opt)
    console.log("File root for", id, "is", lst)
}

// Upload file to IPFS and calculate the root hash

async function doDeploy() {
    var file_id = await createFile("state.data", [])
    var send_opt = {gas:4700000, from:config.base}
    console.log(send_opt, file_id)
    var init_hash = "0xc555544c6083b2a311c8999924893ced050615f712e240c7c439a7d8248226dc"
    var code_address = "QmdFE8Wcj7q6447q2sZ7ttNkCxK5TcmHLFQe1j714k6jTY"
    var contract = await new web3.eth.Contract(abi).deploy({data: code, arguments:[config.tasks, config.fs, file_id, code_address, init_hash]}).send(send_opt)
    contract.setProvider(w3provider)
    config.plasma = contract.options.address
    console.log(JSON.stringify(config))
    var tx = await contract.methods.deposit().send({gas:4700000, from:config.base, value: 100000000})
    console.log("deposit", tx)
    var dta = await contract.methods.debugBlock(1).call(send_opt)
    console.log("what happened", dta)
    var tx = await contract.methods.validateDeposit(1).send({gas:4700000, from:config.base})
    console.log("submitted task", tx)
    contract.events.GotFiles(function (err,ev) {
        console.log("Files", ev.returnValues)
        var files = ev.returnValues.files
        files.forEach(outputFile)
    })
    // process.exit(0)
}

doDeploy()


