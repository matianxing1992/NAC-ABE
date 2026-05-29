# NAC-ABE: Named-based Access Control with Attribute-based Encryption

## 1. Overview

Named Data Networking Attribute-based Encryption Support Library: **NAC-ABE**

The publication of this work is [NAC: Automating Access Control via Named Data](https://arxiv.org/abs/1902.09714) on IEEE MILCOM 2018.
To cite the work, you can use the following Bibtex entry.

```latex
@inproceedings{zhang2018nac,
  title={NAC: Automating access control via Named Data},
  author={Zhang, Zhiyi and Yu, Yingdi and Ramani, Sanjeev Kaushik and Afanasyev, Alex and Zhang, Lixia},
  booktitle={MILCOM 2018-2018 IEEE Military Communications Conference (MILCOM)},
  pages={626--633},
  year={2018},
  organization={IEEE}
}
```

## 2. Quick Start

### 2.1 Dependency

#### 2.1.1 ndn-cxx

NAC-ABE is implemented over the Named Data Networking.
To install the NAC-ABE library, you need to first install [ndn-cxx library](https://github.com/named-data/ndn-cxx).

To work with the version 0.1.0, please checkout `ndn-cxx-0.7.0` and install.

To work with the master version, please checkout `ndn-cxx-0.8.0` and install.

#### 2.1.2 openabe

NAC-ABE is using cryptography support provided by library openabe.
To install openabe, you can visit the [website](https://github.com/zeutro/openabe).

OpenABE version note:

OpenABE is sensitive to the OpenSSL version used at build and run time. The
current NAC-ABE integration has been tested most reliably with OpenABE built
against OpenSSL 1.1.x, such as the OpenSSL version shipped by Ubuntu 20.04.
Ubuntu 22.04 and 24.04 use OpenSSL 3 by default, and an unmodified OpenABE build
may fail to compile, link, or run because upstream OpenABE still contains older
OpenSSL initialization and cleanup code. If you build on Ubuntu 22.04/24.04,
prefer one of these approaches:

```text
1. Build OpenABE with a private OpenSSL 1.1 installation and point NAC-ABE
   CMake to that OpenABE include/lib directory.
2. Use a patched OpenABE fork that supports OpenSSL 3.
```

Do not replace the system OpenSSL with OpenSSL 1.1 on newer Ubuntu systems.
Instead, keep OpenSSL 1.1 private to OpenABE/NAC-ABE under a dedicated OpenABE
prefix.

This repository includes an installer that automates that path:

```bash
./install_nac_abe_stack.sh
```

The installer first checks whether `libopenabe.so` is available under the
OpenABE prefix. If it is, NAC-ABE is configured and built against that OpenABE.
If OpenABE is missing, the installer clones OpenABE into `dependencies/openabe`,
builds OpenABE's bundled private OpenSSL 1.1 dependency, installs OpenABE under
`/usr/local/openabe` by default, copies OpenABE's bundled runtime libraries under
that same prefix, and then configures NAC-ABE with:

```text
-DOPENABE_ROOT=/usr/local/openabe
-DOPENABE_RELIC_ROOT=/usr/local/openabe/deps/root
```

Useful variants:

```bash
./install_nac_abe_stack.sh --no-install
./install_nac_abe_stack.sh --no-tests
./install_nac_abe_stack.sh --no-ldconfig
./install_nac_abe_stack.sh --openabe-only
OPENABE_REPO_URL=https://github.com/matianxing1992/openabe.git ./install_nac_abe_stack.sh --force-openabe
```

By default, the installer configures and builds NAC-ABE unit tests, then runs
them with `ctest --test-dir build --output-on-failure`. This is the supported
unit-test entry point. The test executable is generated under
`build/tests/unit-tests`, but it is a build artifact used by CTest and should
not be treated as the installer validation command. Use `--no-tests` to skip
this validation step.
The installer registers `/usr/local/openabe/lib` and
`/usr/local/openabe/deps/root/lib` through `/etc/ld.so.conf.d/openabe.conf` and
runs `ldconfig`. This keeps OpenABE/RELIC out of the generic `/usr/local/lib`
directory while making them visible to NAC-ABE, NDNSF, and other downstream
programs without per-project `LD_LIBRARY_PATH` or RPATH tweaks. Use
`--no-ldconfig` only if you plan to manage the runtime library path yourself.

By default the installer uses `https://github.com/matianxing1992/openabe.git`,
which carries compatibility fixes for OpenABE's bundled RELIC 0.5.0 build on
newer GCC/CMake toolchains. Override `OPENABE_REPO_URL` only if you intentionally
want to test another OpenABE source tree.

If a newer Ubuntu compiler rejects older OpenABE/RELIC code, use a local
compiler override for this build only:

```bash
./install_nac_abe_stack.sh --cc gcc-10 --cxx g++-10 --force-openabe
```

This sets `CC` and `CXX` only for the installer and its child builds. It does
not change `/usr/bin/gcc`, does not replace system packages, and should not
affect other software on the machine. Prefer GCC 10 or 11 over very old
compilers, because NAC-ABE and ndn-cxx still require a C++17-capable toolchain.

For manual CMake builds against the dedicated OpenABE prefix:

```bash
cmake -S . -B build \
  -DOPENABE_ROOT=/usr/local/openabe \
  -DOPENABE_RELIC_ROOT=/usr/local/openabe/deps/root
cmake --build build -j"$(nproc)"
```

> (As noticed in July 7, 2020)
> When installing the OpenABE, there could be some issues installing gTest (on Ubuntu) or Bison 3.3 (on MacOS).
> While waiting for the OpenABE maintainer to fix them, as a quick solution, you can fix these issues manually.

### 2.2 Install NAC-ABE

Really simple to make it using waf.

#### 2.2.1 Download

```bash
git clone https://github.com/UCLA-IRL/NAC-ABE.git
```

#### 2.2.2 Building

NAC-ABE support building by both CMake and waf build.

##### 2.2.2.1 CMake build

We start by configuring:

```bash
# in the root directory of NAC-ABE
mkdir build && cd build
cmake ..
```

Or if you want to enable tests:

```bash
mkdir build && cd build
cmake -DHAVE_TESTS=True ..
```

Then compile and install:

```bash
make
make install
```

##### 2.2.2.2 waf build (deprecated)

We start by configuring:
```bash
# in the root directory of NAC-ABE
./waf configure
```
or if you want to enable tests.
```bash
./waf configure --with-tests
```
Then compile with
```bash
./waf
```
And install by
```bash
./waf install
```

### 2.3 Run Tests

To run tests, you must have `-DHAVE_TESTS=True` when you config the project.

```bash
# from the NAC-ABE source directory
cmake -S . -B build -DHAVE_TESTS=True
cmake --build build --target unit-tests
ctest --test-dir build --output-on-failure
```

The CMake test target copies the test trust schema and trust anchor into the
build directory automatically and runs the test with the build directory as its
working directory. Prefer `ctest --test-dir build --output-on-failure` over
running `build/tests/unit-tests` directly, because the tests intentionally use
relative trust-schema and trust-anchor files prepared by CMake/CTest. The
installer restores those test assets around its own CTest run so the build tree
remains ready for CTest-based validation.

Build-tree unit tests are linked so that they prefer `build/libnac-abe.so`
before any previously installed `/usr/local/lib/libnac-abe.so`. Installed
downstream programs should not need per-command `LD_LIBRARY_PATH`; the installer
registers OpenABE/RELIC paths with `ldconfig` and installs NAC-ABE normally.

The CMake test target also uses CMake's FindBoost module for tests, which avoids
stale `BoostConfig.cmake` files under `/usr/local` on Ubuntu 20.04/22.04 and
links the required Boost.Test, Boost.Filesystem, and Boost.System libraries
explicitly.

### 2.4 Run Examples

To run example, you must have `-DBUILD_EXAMPLES=True` when you config the project.

```bash
# in the examples directory of NAC-ABE
# nfd-start & (if your NFD has not started)
bash run-examples.sh ../build
```

## 3 Documentation

The library mainly provide supports for four roles in an NDN based ABE scenario.

* **Attribute Authority**. The party who owns the system master key. It publishes the public parameters to the system and generate decryption keys for decryptors.
* **Data owner**. The party who decides how encryptors should encrypt their data.
* **Encryptor**. The party who follows data owner's decision and produce encrypted data.
* **Decryptor**. The party who get decryption keys from the attribute authority and consume encrypted data.

These four parties are implemented in five classes in the library: `CpAttributeAuthority`, `KpAttributeAuthority`, `DataOwner`, `producer`, and `consumer`.

> For now, Both Ciphertext Policy Attribute-based Encryption (CP-ABE) and Key Policy Attribute-based Encryption (KP-ABE) is supported.

From the perspective of the data flow:

* Content is encrypted by the content KEY (CK), which is a symmetric AES key
* CK is encrypted by the attribute policy (EKEY)
* CK can only be decrypted when the attributes (DKEY) can satisfy the EKEY
* Decryptor obtains an DKEY from attribute authorities
* Encryptor knows which EKEY to use from the data owner

### 3.1 Attribute Authority

#### Instantiate a new attribute authority

```c++
// obtain or create a certificate for attribute authority for CP-ABE
CpAttributeAuthority aa(aaCert, face, keychain);

// obtain or create a certificate for attribute authority for KP-ABE
KpAttributeAuthority aa(aaCert, face, keychain);
```

#### Add policy

Add a new decryptor and its corresponding attribute list into authority:

```c++
// obtain the decryptor's certificate for CP-ABE
std::list<std::string> attrList = {"ucla", "professor"};
aa.addNewPolicy(decryptorCertificate, attrList);

// obtain the decryptor's certificate for KP-ABE
String policy = "ucla and cs and (exam or quiz);
aa.addNewPolicy(decryptorCertificate, policy);
```

After starting an attribute authority and added policies for decryptors,
the attribute authority will listen to the prefix `/<attribute authority prefix>/DKEY` to answer possible attribute request from decryptors.
When a request arrives, the attribute authority will first use a known decryptor cetificate to verify the request, then locates the attribtue lists of this decryptor.
After that, the attribute authority will create a new AES key to cpEncrypt the attributes, then use the decryptor's RSA public key from the certificate to cpEncrypt the AES key.
The encrypted attributes will be returned back to the decryptor.

### 3.2 Data Owner

#### Instantiate a new data owner

```c++
// obtain or create a certificate for data owner
DataOwner dataOwner = DataOwner(dataOwnerCert, face, keychain);
```

#### Command a data producer

Command a data producer to apply certain policy when producing certain Data packets.

```c++
//for CP-ABE
dataOwner.commandProducerPolicy(Name("/producer"), Name("/healthdata"), "ucla and professor", successCallback, failCallback);

//for KP-ABE
dataOwner.commandProducerPolicy(Name("/producer"), Name("/healthdata"), {"ucla", "cs", "exam"}, successCallback, failCallback);
```

To command a data producer, the data owner will use its own private key to sign the command Interest.
The command Interest is of format: `/<producer prefix>/SET_POLICY/<data prefix block>/<policy string>`.

### 3.3 Encryptor (Data Producer)

#### Instantiate a new encryptor

```c++
Producer producer = Producer(face, keychain, producerCert, aaCert, dataOwnerCert);
```

After starting a encryptor, the encryptor will automatically fetch the public parameters from the attribute authority.

After starting a encryptor, the encryptor will listen to the prefix `/<producer prefix>/SET_POLICY` for the data owner to command the policy.
When a command Interest arrives, the encryptor will verify the command Interest with the data owner's certificate.

#### Produce a new data

```c++
std::shared_ptr<Data> contentData, ckData;
std::tie(contentData, ckData) = producer.produce(dataName, PLAIN_TEXT);
```

This function will automatically find the policy that is previously obtained from the command issued by the data owner.

The encryptor can also produce a new data using a new policy:

```c++
std::shared_ptr<Data> contentData, ckData;
// for CP-ABE
std::tie(contentData, ckData) = producer.produce(dataName, "ucla and professor", PLAIN_TEXT);
// for KP-ABE
std::tie(contentData, ckData) = producer.produce(dataName, {"ucla", "cs", "exam"}, PLAIN_TEXT);
```

#### Reusing content keys

To produce multiple data under the same content key (thus the same policy), use the content key generation API:

```c++
std::shared_ptr<ContentKey> contentKey;
std::shared_ptr<Data> ckData;
// for CP-ABE
std::tie(contentKey, ckData) = producer.ckDataGen(dataName, "ucla and professor", PLAIN_TEXT);
// for KP-ABE
std::tie(contentKey, ckData) = producer.ckDataGen(dataName, {"ucla","cs","exam"}, PLAIN_TEXT);
```

Then, for each Data:

```c++
std::shared_ptr<Data> contentData;
// for CP-ABE
contentData = producer.produce(contentKey, ckData.getName(), dataName, "ucla and professor", PLAIN_TEXT);
// for KP-ABE
contentData = producer.produce(contentKey, ckData.getName(), dataName, {"ucla","cs","exam"}, PLAIN_TEXT);
```

You can also use CacheProducer, which will automatically cache and reuse all the keys generated.

### 3.4 Decryptor (Data Consumer)

#### Instantiate a new decryptor

```c++
Consumer consumer = Consumer(face, keyChain, consumerCert, aaCert);
```

After starting a decryptor, the decryptor will automatically fetch the public parameters from the attribute authority.

#### Obtain the decryption key (DKEY), i.e., attributes

```c++
consumer.obtainDecryptionKey();
```

This function will fetch the DKEY from the attribute authority.

#### Consume data

```c++
consumer.consume(dataName, successCallback, failCallback);
```

This function will fetch the content Data packet by the name, fetch the corresponding encrypted CK Data packet.
Then it uses its decryption key (DKEY) to cpDecrypt the CK, then use the CK to cpDecrypt the content Data packet.

## 4 Issue Report

If you have any problems or want to do bug report.
Please submit a GitHub issue.
