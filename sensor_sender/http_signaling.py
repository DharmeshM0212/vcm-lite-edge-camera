import asyncio
from typing import Any

import aiohttp


class HttpSignaling:
    def __init__(self, base_url: str) -> None:
        self.base_url = base_url.rstrip("/")

    async def reset(self) -> None:
        async with aiohttp.ClientSession() as session:
            async with session.post(f"{self.base_url}/reset") as response:
                response.raise_for_status()

    async def write_offer(self, value: dict[str, Any]) -> None:
        async with aiohttp.ClientSession() as session:
            async with session.post(f"{self.base_url}/offer", json=value) as response:
                response.raise_for_status()

    async def wait_for_answer(self) -> dict[str, Any]:
        async with aiohttp.ClientSession() as session:
            while True:
                async with session.get(f"{self.base_url}/answer") as response:
                    response.raise_for_status()
                    data = await response.json()

                if data.get("available") and isinstance(data.get("answer"), dict):
                    return data["answer"]

                await asyncio.sleep(0.25)