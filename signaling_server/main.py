from typing import Any

from fastapi import FastAPI
from pydantic import BaseModel


app = FastAPI(title="VCM-Lite WebRTC Signaling Server")


class SessionDescription(BaseModel):
    sdp: str
    type: str


state: dict[str, Any] = {
    "offer": None,
    "answer": None
}


@app.get("/health")
def health() -> dict[str, Any]:
    return {
        "status": "ok",
        "offer_available": state["offer"] is not None,
        "answer_available": state["answer"] is not None
    }


@app.post("/reset")
def reset() -> dict[str, Any]:
    state["offer"] = None
    state["answer"] = None
    return {"status": "reset"}


@app.post("/offer")
def post_offer(offer: SessionDescription) -> dict[str, Any]:
    state["offer"] = offer.model_dump()
    state["answer"] = None
    return {"status": "offer_stored"}


@app.get("/offer")
def get_offer() -> dict[str, Any]:
    return {
        "available": state["offer"] is not None,
        "offer": state["offer"]
    }


@app.post("/answer")
def post_answer(answer: SessionDescription) -> dict[str, Any]:
    state["answer"] = answer.model_dump()
    return {"status": "answer_stored"}


@app.get("/answer")
def get_answer() -> dict[str, Any]:
    return {
        "available": state["answer"] is not None,
        "answer": state["answer"]
    }