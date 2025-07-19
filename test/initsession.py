import asyncio
import os

import xeddsa
import x3dh
from oldmemo import oldmemo
from twomemo import twomemo
import omemo
from omemo.storage import Maybe, JSONType, Nothing

OMEMO2=os.getenv("OMEMO2") is not None

if OMEMO2:
    import bundle2
else:
    import bundle


class StorageImpl(omemo.storage.Storage):
    def __init__(self) -> None:
        super().__init__()
        self.__data: Dict[str, JSONType] = {}

    async def _load(self, key: str) -> Maybe[JSONType]:
        if key in self.__data:
            return Just(self.__data[key])
        return Nothing()

    async def _store(self, key: str, value: JSONType) -> None:
        self.__data[key] = value

    async def _delete(self, key: str) -> None:
        self.__data.pop(key, None)


async def run_oldmemo():
    ik=xeddsa.curve25519_pub_to_ed25519_pub(oldmemo.StateImpl.parse_public_key(bundle.ik), bool((bundle.spks[63] >> 7) & 1))

    spks=bytearray(bundle.spks)
    spks[63] &= 0x7f
    pks= { oldmemo.StateImpl.parse_public_key(v):k
          for k, v in bundle.pks.items()}
    b=oldmemo.BundleImpl(
        "admin@localhost",7,
        x3dh.Bundle(
            ik,
            oldmemo.StateImpl.parse_public_key(bundle.spk),
            bytes(spks),
            {pk for pk in pks.keys()}
            ),
        bundle.spk_id,
        pks,
    )
    o=oldmemo.Oldmemo(StorageImpl())
    k=oldmemo.PlainKeyMaterialImpl(b"\x55"*16,b"\xaa"*16)
    ses, msg = await o.build_session_active("user@localhost", 8, b, k)
    ser,sign=ses.key_exchange.serialize(msg.serialize())
    with open("o/msg.bin", "wb") as f:
        f.write(ser)

async def run_twomemo():
    pks = { v:k for k, v in bundle2.pks.items()}
    b=twomemo.BundleImpl(
        "admin@localhost",7,
        x3dh.Bundle(
            bundle2.ik,
            bundle2.spk,
            bundle2.spks,
            {pk for pk in pks.keys()}
            ),
        bundle2.spk_id,
        pks,
    )
    o=twomemo.Twomemo(StorageImpl())
    k=twomemo.PlainKeyMaterialImpl(b"\x55"*32,b"\xaa"*16)
    ses, msg = await o.build_session_active("user@localhost", 8, b, k)
    ser=ses.key_exchange.serialize(msg.serialize())
    with open("o/msg2.bin", "wb") as f:
        f.write(ser)
    pass

async def main():
    if OMEMO2:
        await run_twomemo()
    else:
        await run_oldmemo()

asyncio.run(main())
