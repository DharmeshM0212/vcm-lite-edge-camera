import asyncio
from typing import Any

import aiohttp


class HttpSignaling:
    def __init__(self, base_url: str) -> None:
        self.base_url = base_url.rstrip("/")

    async def wait_for_offer(self) -> dict[str, Any]:
        async with aiohttp.ClientSession() as session:
            while True:
                async with session.get(f"{self.base_url}/offer") as response:
                    response.raise_for_status()
                    data = await response.json()

                if data.get("available") and isinstance(data.get("offer"), dict):
                    return data["offer"]

                await asyncio.sleep(0.25)

    async def write_answer(self, value: dict[str, Any]) -> None:
        async with aiohttp.ClientSession() as session:
            async with session.post(f"{self.base_url}/answer", json=value) as response:
                response.raise_for_status()