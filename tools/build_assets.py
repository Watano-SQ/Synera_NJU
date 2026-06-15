from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageOps, ImageSequence


ROOT = Path(__file__).resolve().parents[1]
PICTURES = ROOT / "pictures"
ASSETS = ROOT / "assets"


# Unit visuals include base plants and star-up looks. This is not the shop unit pool.
UNIT_ASSETS = {
    "豌豆射手.gif": "units/peashooter.png",
    "双重射手.gif": "units/repeater.png",
    "向日葵.gif": "units/sunflower.png",
    "双胞向日葵.gif": "units/twin_sunflower.png",
    "坚果.gif": "units/wallnut.png",
    "高坚果.gif": "units/tallnut.png",
    "PuffShroom.gif": "units/puffshroom.png",
    "ScaredyShroom.gif": "units/scaredyshroom.png",
    "FumeShroom.gif": "units/fumeshroom.png",
    "GloomShroom.gif": "units/gloomshroom.png",
    "Spikeweed.gif": "units/spikeweed.png",
    "Spikerock.gif": "units/spikerock.png",
}

ENEMY_ASSETS = {
    "Zombie.gif": "enemies/zombie.png",
    "ConeheadZombie.gif": "enemies/conehead_zombie.png",
    "ScreenDoorZombie.gif": "enemies/screendoor_zombie.png",
    "NewspaperZombie.gif": "enemies/newspaper_zombie.png",
    "FootballZombie.gif": "enemies/football_zombie.png",
    "JackinTheBoxZombie.gif": "enemies/jackbox_zombie.png",
}

ITEM_ASSETS = {
    "TreeFood.png": "items/plant_food.png",
    "Pumpkin_damage3_allstar.png": "items/pumpkin_shell.png",
    "Zen_GardenGlove.png": "items/garden_glove.png",
    "Plantern_body.png": "items/chlorophyll.png",
}

UI_ASSETS = {
    "植物商店.png": "ui/plant_shop.png",
    "按钮.png": "ui/button.png",
    "阳关计数.png": "ui/sun_counter.png",
    "脑子.png": "ui/brain.png",
    "FlagMeterEmpty.png": "ui/flag_meter_empty.png",
    "FlagMeterFull.png": "ui/flag_meter_full.png",
    "Zombieline.jpg": "ui/zombieline.jpg",
}

BACKGROUND_ASSETS = {
    "白天.jpg": "backgrounds/day_board.jpg",
    "晚上.jpg": "backgrounds/night_board.jpg",
}

# Shop cards are seed-packet art for purchasable base plants only.
SHOP_CARD_ASSETS = {
    "peashooter": "Peashooter.png",
    "sunflower": "SunFlower.png",
    "wallnut": "HugeWallNut.png",
    "puffshroom": "PuffShroom.png",
    "fumeshroom": "FumeShroom.png",
    "spikeweed": "Spikeweed.png",
}

TRAIT_BADGES = {
    "shooter": ("射", "#7fbf3f", "#18380d"),
    "nut": ("坚", "#b98b42", "#3f260a"),
    "sun": ("阳", "#ffd34e", "#5c3b00"),
    "healer": ("愈", "#f78fb3", "#51152a"),
    "fungus": ("菇", "#8e63ce", "#24133f"),
    "spike": ("刺", "#86a34a", "#1f2b10"),
}


def ensure_parent(relative_output: str) -> Path:
    path = ASSETS / relative_output
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def load_first_frame(path: Path) -> Image.Image:
    with Image.open(path) as image:
        if getattr(image, "is_animated", False):
            frame = next(ImageSequence.Iterator(image))
            return frame.convert("RGBA")
        return image.convert("RGBA")


def trim_transparent(image: Image.Image) -> Image.Image:
    alpha = image.getchannel("A")
    bbox = alpha.getbbox()
    if bbox is None:
        return image
    return image.crop(bbox)


def save_bottom_centered(source: str, output: str, size: int = 96) -> None:
    input_path = PICTURES / source
    if not input_path.exists():
        raise FileNotFoundError(f"Missing input asset: {input_path}")

    image = trim_transparent(load_first_frame(input_path))
    image.thumbnail((size - 8, size - 8), Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    x = (size - image.width) // 2
    y = size - image.height - 4
    canvas.alpha_composite(image, (x, y))
    canvas.save(ensure_parent(output))


def save_icon(source: str, output: str, size: int = 64) -> None:
    input_path = PICTURES / source
    if not input_path.exists():
        raise FileNotFoundError(f"Missing input asset: {input_path}")

    image = trim_transparent(load_first_frame(input_path))
    image.thumbnail((size - 8, size - 8), Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    x = (size - image.width) // 2
    y = (size - image.height) // 2
    canvas.alpha_composite(image, (x, y))
    canvas.save(ensure_parent(output))


def save_ui(source: str, output: str) -> None:
    input_path = PICTURES / source
    if not input_path.exists():
        raise FileNotFoundError(f"Missing input asset: {input_path}")

    image = load_first_frame(input_path)
    output_path = ensure_parent(output)
    if output_path.suffix.lower() in {".jpg", ".jpeg"}:
        image.convert("RGB").save(output_path, quality=92)
    else:
        image.save(output_path)


def save_plant_shop_sun_slot() -> None:
    input_path = PICTURES / "植物商店.png"
    if not input_path.exists():
        raise FileNotFoundError(f"Missing input asset: {input_path}")

    image = load_first_frame(input_path)
    # The current source is 446x87; the left 116px keeps the full sun slot
    # at roughly the same 4:3 ratio used by ShopPanel's 72x54 target.
    crop = image.crop((0, 0, min(116, image.width), image.height))
    crop.save(ensure_parent("ui/plant_shop_sun_slot.png"))


def save_zombieline_tray() -> None:
    input_path = PICTURES / "Zombieline.jpg"
    if not input_path.exists():
        raise FileNotFoundError(f"Missing input asset: {input_path}")

    with Image.open(input_path) as image:
        rgb = image.convert("RGB")
        # Remove the thin unused bottom edge while keeping the visible tray art.
        cropped = rgb.crop((0, 0, rgb.width, max(1, rgb.height - 2)))
        cropped.save(ensure_parent("ui/zombieline_tray.jpg"), quality=92)


def save_background(source: str, output: str, size: tuple[int, int] = (640, 640)) -> None:
    input_path = PICTURES / source
    if not input_path.exists():
        raise FileNotFoundError(f"Missing input asset: {input_path}")

    with Image.open(input_path) as image:
        rgb = image.convert("RGB")
        cropped = ImageOps.fit(rgb, size, method=Image.Resampling.LANCZOS, centering=(0.5, 0.5))
        cropped.save(ensure_parent(output), quality=92)


def save_shop_card(definition_id: str, source: str) -> None:
    input_path = PICTURES / source
    if not input_path.exists():
        raise FileNotFoundError(f"Missing input asset: {input_path}")

    image = load_first_frame(input_path)
    midpoint = image.height // 2
    available = image.crop((0, 0, image.width, midpoint))
    disabled = image.crop((0, midpoint, image.width, image.height))
    available.save(ensure_parent(f"shop_cards/{definition_id}_available.png"))
    disabled.save(ensure_parent(f"shop_cards/{definition_id}_disabled.png"))


def load_font(size: int) -> ImageFont.ImageFont:
    candidates = [
        Path("C:/Windows/Fonts/msyh.ttc"),
        Path("C:/Windows/Fonts/simhei.ttf"),
        Path("C:/Windows/Fonts/arial.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size=size)
    return ImageFont.load_default()


def save_trait_badge(trait_id: str, text: str, fill: str, ink: str) -> None:
    size = 64
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw.ellipse((4, 4, size - 5, size - 5), fill=fill, outline=(255, 255, 255, 230), width=3)
    draw.ellipse((9, 9, size - 10, size - 10), outline=(0, 0, 0, 70), width=2)

    font = load_font(28)
    bbox = draw.textbbox((0, 0), text, font=font)
    x = (size - (bbox[2] - bbox[0])) // 2 - bbox[0]
    y = (size - (bbox[3] - bbox[1])) // 2 - bbox[1] - 1
    draw.text((x, y), text, fill=ink, font=font)
    canvas.save(ensure_parent(f"traits/{trait_id}.png"))


def main() -> None:
    for folder in ["units", "enemies", "items", "traits", "ui", "backgrounds", "shop_cards"]:
        (ASSETS / folder).mkdir(parents=True, exist_ok=True)

    for source, output in UNIT_ASSETS.items():
        save_bottom_centered(source, output)
    for source, output in ENEMY_ASSETS.items():
        save_bottom_centered(source, output)
    for source, output in ITEM_ASSETS.items():
        save_icon(source, output)
    for source, output in UI_ASSETS.items():
        save_ui(source, output)
    save_plant_shop_sun_slot()
    save_zombieline_tray()
    for source, output in BACKGROUND_ASSETS.items():
        save_background(source, output)
    for definition_id, source in SHOP_CARD_ASSETS.items():
        save_shop_card(definition_id, source)
    for trait_id, (text, fill, ink) in TRAIT_BADGES.items():
        save_trait_badge(trait_id, text, fill, ink)

    print(f"Assets written to {ASSETS}")


if __name__ == "__main__":
    main()
