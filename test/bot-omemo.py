# from https://github.com/Syndace/slixmpp-omemo/blob/main/examples/echo_client.py

from argparse import ArgumentParser
from getpass import getpass
import json
import logging
import sys
from typing import Any, Dict, FrozenSet, Literal, Optional, Union

from omemo.storage import Just, Maybe, Nothing, Storage
from omemo.types import DeviceInformation, JSONType

from slixmpp.clientxmpp import ClientXMPP
from slixmpp.jid import JID
from slixmpp.plugins import register_plugin  # type: ignore[attr-defined]
from slixmpp.stanza import Message
from slixmpp.xmlstream.handler import CoroutineCallback
from slixmpp.xmlstream.matcher import MatchXPath

from slixmpp_omemo import TrustLevel, XEP_0384

import traceback
import random

import asyncio


log = logging.getLogger(__name__)


class StorageImpl(Storage):
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

class XEP_0384Impl(XEP_0384):  # pylint: disable=invalid-name
    def __init__(self, *args: Any, **kwargs: Any) -> None:  # pylint: disable=redefined-outer-name
        super().__init__(*args, **kwargs)
        self.__storage: Storage

    def plugin_init(self) -> None:
        self.__storage = StorageImpl()
        super().plugin_init()

    @property
    def storage(self) -> Storage:
        return self.__storage

    @property
    def _btbv_enabled(self) -> bool:
        return True

    #async def _devices_blindly_trusted(
    #    self,
    #    blindly_trusted: FrozenSet[DeviceInformation],
    #    identifier: Optional[str]
    #) -> None:
    #    log.info(f"[{identifier}] Devices trusted blindly: {blindly_trusted}")

    async def _prompt_manual_trust(
        self,
        manually_trusted: FrozenSet[DeviceInformation],
        identifier: Optional[str]
    ) -> None:
        session_mananger = await self.get_session_manager()
        for device in manually_trusted:
            await session_mananger.set_trust(
                device.bare_jid,
                device.identity_key,
                TrustLevel.TRUSTED.value
            )


register_plugin(XEP_0384Impl)


class OmemoEchoClient(ClientXMPP):
    def __init__(self, jid: str, password: str) -> None:
        super().__init__(jid, password)
        self.add_event_handler("session_start", self.start)
        self.register_handler(CoroutineCallback(
            "Messages",
            MatchXPath(f"{{{self.default_ns}}}message"),
            self.message_handler  # type: ignore[arg-type]
        ))

    def start(self, _event: Any) -> None:
        self.send_presence()
        self.get_roster()  # type: ignore[no-untyped-call]

    async def message_handler(self, stanza: Message) -> None:
        xep_0384: XEP_0384 = self["xep_0384"]
        mto = stanza["from"]
        mtype = stanza["type"]
        body = stanza["body"]
        if mtype not in { "chat", "normal" }:
            return

        namespace = xep_0384.is_encrypted(stanza)
        if namespace is None:
            if body == "exit":
                exit()
            print("plain: " + body)
            stanza = self.make_message(mto=mto, mtype=mtype)
            stanza["body"] = body
            stanza.send()
            return

        try:
            message, device_information = await xep_0384.decrypt_message(stanza)
        except Exception as e:  # pylint: disable=broad-exception-caught
            print("Exception", traceback.format_exc())
            return

        try:
            await self.encrypted_reply(mto, mtype, message)
        except Exception as e:  # pylint: disable=broad-exception-caught
            print("Exception", traceback.format_exc())

    async def encrypted_reply(
        self,
        mto: JID,
        mtype: Literal["chat", "normal"],
        reply: Union[Message, str]
    ) -> None:
        xep_0384: XEP_0384 = self["xep_0384"]
        if isinstance(reply, str):
            msg = reply
            reply = self.make_message(mto=mto, mtype=mtype)
            reply["body"] = msg

        if reply["body"] == "exit":
            exit()
        print("omemo: " + reply["body"])

        reply.set_to(mto)
        reply.set_from(self.boundjid)

        messages, encryption_errors = await xep_0384.encrypt_message(reply, mto)

        if len(encryption_errors) > 0:
            log.info(f"There were non-critical errors during encryption: {encryption_errors}")

        for namespace, message in messages.items():
            message["eme"]["namespace"] = namespace
            message["eme"]["name"] = self["xep_0380"].mechanisms[namespace]
            message.send()


if __name__ == "__main__":
    xmpp = OmemoEchoClient("user@localhost", "userpass")
    xmpp.register_plugin("xep_0380")
    xmpp.register_plugin("xep_0384", module=sys.modules[__name__])
    xmpp.connect(disable_starttls=True)
    asyncio.get_event_loop().run_forever()
