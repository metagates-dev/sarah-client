-- sha function test program
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

-- sha1 testing
local sha1_test = {
    {"", "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
    {"abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
    {"123", "40bd001563085fc35165329ea1ff5c5ecbdbbeef"},
    {"this is a string", "517592df8fec3ad146a79a9af153db2a4d784ec5"}
};


local k, v;
local sha_val = "";

print("+---sha1 testing: \n");
for _, item in ipairs(sha1_test) do
    k = item[1];
    v = item[2];
    sha_val = sha1(k);
    print("sha1(", k, ")=", sha_val, " ... ");
    if ( sha_val == v ) then
        print(" --[Ok]\n");
    else
        print(" --[Failed]\n");
    end
end




-- sha256 testing
local sha256_test = {
    {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
    {"123", "a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3"},
    {"this is a string", "bc7e8a24e2911a5827c9b33d618531ef094937f2b3803a591c625d0ede1fffc6"}
};

print("+---sha256 testing: \n");
for _, item in ipairs(sha256_test) do
    k = item[1];
    v = item[2];
    sha_val = sha256(k);
    print("sha256(", k, ")=", sha_val, " ... ");
    if ( sha_val == v ) then
        print(" --[Ok]\n");
    else
        print(" --[Failed]\n");
    end
end




-- sha512 testing
local sha512_test = {
    {"", "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"},
    {"abc", "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
    {"123", "3c9909afec25354d551dae21590bb26e38d53f2173b8d3dc3eee4c047e7ab1c1eb8b85103e3be7ba613b31bb5c9c36214dc9f14a42fd7a2fdb84856bca5c44c2"},
    {"this is a string", "7edf513d44ffdb0797ad802ad9a017af32d6f16be78be5942133f5f40b2b03cffe8a24f9671b6b0f8c28f2f235b1bfb43cc880a8fbcc3167fbc77d453ce4d6dc"}
};

print("+---sha512 testing: \n");
for _, item in ipairs(sha512_test) do
    k = item[1];
    v = item[2];
    sha_val = sha512(k);
    print("sha512(", k, ")=", sha_val, " ... ");
    if ( sha_val == v ) then
        print(" --[Ok]\n");
    else
        print(" --[Failed]\n");
    end
end
