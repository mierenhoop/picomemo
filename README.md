## About

This repository contains a compact and portable implementation of the
cryptography required for XMPP's OMEMO (E2EE).

### Features

- Portable, even runs on embedded systems like the ESP32!

- Compatible with other XMPP clients that support OMEMO.

- Low amount of code with few dependencies.

- High performance, uses fast crypto when available.

## `omemo.c`

`omemo.c` contains implementations of X3DH, Double Ratchet and
Protobuf with an API that is specifically tailored to OMEMO. Without
dependencies on (any) libsignal or libolm code.

Both OMEMO 0.3 (`eu.siacs.conversations.axolotl`) and OMEMO 0.9
(`urn:xmpp:omemo:2`) are supported. By default OMEMO 0.3 is enabled and
when compiled with `-DOMEMO2`, OMEMO 0.9 is enabled. That means only
one is active at a time. It is possible to include both versions in a
client by linking both separately as a shared library as is done in
the Lua bindings, or by compiling with `-DOMEMO_EXPORT=static` and
directly including `omemo.c` while somehow reexporting the API.

### Crypto functions

There are two "backends" for the underlying Curve25519 and EdDSA
cryptography. By default these are provided by
[HACL\*](https://github.com/hacl-star/hacl-star) as an amalgamation in
`hacl.c`. These are both fast and formally verified.

As an alternative there is the
[c25519](https://www.dlbeer.co.nz/oss/c25519.html) library, which is
also included as amalgamation in `/c25519.c` and `/c25519.h`. This
library was designed for low-memory systems and is significantly slower
on modern hardware than HACL\*.

## `lomemo`

In the `lua` subdirectory there are Lua bindings for this library. With
support for Lua 5.1 up to at least Lua 5.4.

## Dependencies

- MbedTLS 3.0+

- C compiler (gcc)

- docker-compose (for testing)

- Lua 5.1-5.4 (for Lua bindings)

## Usage

Running the tests:

 `$ make test-omemo`

**Using this library in your project:**

Copy over the source files, `omemo.c/h` are mandatory and you have to
choose either `c25519.c/h` or `hacl.c/h`.

You must link against libmbedcrypto (and/or configure your mbedtls build
to only include the required functions.

## API

Refer to `omemo.h` for function definitions and function-specific
documentation.

To give an understanding how the API can be integrated in a client, here
is some pseudocode for a rough overview of how the functions can be used:

```diff

// Implement random callback (required!)
int omemoRandom(p, n) {
    getrandom(p, n, 0)
    return 0
}

class XmppClient {
    struct omemoStore store
    Map<(Jid, DeviceId), struct omemoSession> sessions

    Setup() {
        // Loading/Saving OMEMO store (your bundle of keys)
        storebin = ReadFile("store.bin")
        if storebin {
>           omemoDeserializeStore(storebin, len(storebin), &store)
        } else {
>           omemoSetupStore(&store)
            file = OpenFile("store.bin")
>           file.buffer_size = omemoGetSerializedStoreSize(&store)
>           omemoSerializeStore(file.buffer, &store)
            // Extract keys/ids from the store for publishing your bundle
            PublishBundle(&store)
        }
        // Key should be rotated once every week to month, remember to
        // save the store to disk after any changes AND publish your new
        // bundle.
>       setInterval(() => omemoRotateSignedPreKey(&store), 1*Week)
    }

    SendEncryptedMessage(to_jid, body) {
        // With OMEMO 0.3 you can omit omemoGetMessagePadSize
        encrypted_buf = malloc(len(body)+omemoGetMessagePadSize(len(body)))
>       omemoEncryptMessage(encrypted_buf, out key_payload, out iv, body,
        len(body))

        // Get device list via (cached) iq stanza
        for device_id in GetDevices(to_jid) {
            session = sessions.Get(to_jid, device_id)
            if not session {
                session = {0}
                bundlexml = FetchBundle(to_id, device_id)
                struct omemoBundle bundle
                FillBundle(&bundle, bundlexml)
>               omemoInitFromBundle(&session, &store, &bundle)
>               sessions.Set(to_jid, device_id, session)
            }
>           omemoEncryptKey(&session, &store, out key_msg, key_payload)
            headers.append(MakeXml(key_msg.isprekey, key_msg.p, key_msg.n))
            // Save session into some database/file just like the store
>           omemoSerializeSession(..., &session)
            // Also save the store the store (not shown)
        }
        SendMessage(MakeXml(headers, encrypted_buf, iv))
    }

    event GotMessage(msg) {
        if msg.is_omemo_encrypted? {
            // For decryption, the functions omemoLoadMessageKey and
            // omemoStoreMessageKey will be called, you must implement
            // them just like omemoRandom at the top level (shown here
            // for demonstration purposes.
>           int omemoLoadMessageKey(session, key) {
                key.mk = skipped_keys[session][key.dh, key.nr]
                return Found?(key.mk) ? 0 : 1
            }

>           int omemoStoreMessageKey(session, key, n) {
                if n > MAX_SKIP_KEYS {
                    return OMEMO_EUSER
                }
                skipped_keys[session][key.dh, key.nr] = key.mk
            }

            // Search the XML of the message for our key and get the
            // prekey/kex attribute
            key, isprekey = FindKeyForOurDevice(msg)
>           omemoDecryptKey(&session, &store, out key_payload, isprekey, key)
            if isprekey {
                // A prekey is used, publish your bundle again
                // without that prekey!
                PublishBundle(&store)
            }
            plaintext = allocate(len(msg.payload))
>           omemoDecryptMessage(plaintext, key_payload, msg.iv, msg.payload, len(msg.payload))
            // Save session and store here again (not shown), also save
            // the skipped keys!
            ShowPlaintextMessage(plaintext)
        }
    }
}

```

## License

The code in this repository is licensed under ISC, all dependencies are also permissively licensed.
