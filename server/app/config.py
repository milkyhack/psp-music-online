from pathlib import Path
import os
from typing import Optional

from pydantic_settings import BaseSettings, SettingsConfigDict

_SERVER_ROOT = Path(__file__).resolve().parent.parent


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=str(_SERVER_ROOT / ".env"),
        env_file_encoding="utf-8",
        extra="ignore",
    )

    music_dir: Path = Path.home() / "Music"
    host: str = "0.0.0.0"
    port: int = 8084
    api_key: str = ""
    lastfm_api_key: str = ""
    data_dir: Path = _SERVER_ROOT / "data"

    @property
    def db_path(self) -> Path:
        return self.data_dir / "library.db"

    @property
    def covers_dir(self) -> Path:
        return self.data_dir / "covers"

    @property
    def cache_dir(self) -> Path:
        return self.data_dir / "transcode_cache"


settings = Settings()


def apply_runtime_settings(
    *,
    music_dir: Optional[Path] = None,
    host: Optional[str] = None,
    port: Optional[int] = None,
    api_key: Optional[str] = None,
) -> None:
    """Keep the live Settings object and process env in sync after .env writes."""
    if music_dir is not None:
        settings.music_dir = music_dir
        os.environ["MUSIC_DIR"] = str(music_dir)
    if host is not None:
        settings.host = host
        os.environ["HOST"] = host
    if port is not None:
        settings.port = int(port)
        os.environ["PORT"] = str(int(port))
    if api_key is not None:
        settings.api_key = api_key
        os.environ["API_KEY"] = api_key
