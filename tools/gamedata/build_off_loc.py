"""Generate a reconstructed gameModeLanguages.loc (StringTable format 2)."""
import argparse
import struct
from pathlib import Path

THEMES = {
    "en-EN": "Castle|Dragon|Treehouse|Pirate Ship|Robot|Space Station|Train|Waterfall|Windmill|Lighthouse|Farm|Airplane|Bridge|Volcano|Snowman|Rocket|Garden|Submarine|Cat|Dog|Library|Temple|Tower|Island|Fortress|Hot Air Balloon|Clock|Monster|Mountain|Village",
    "ru-RU": "Замок|Дракон|Дом на дереве|Пиратский корабль|Робот|Космическая станция|Поезд|Водопад|Ветряная мельница|Маяк|Ферма|Самолёт|Мост|Вулкан|Снеговик|Ракета|Сад|Подводная лодка|Кот|Собака|Библиотека|Храм|Башня|Остров|Крепость|Воздушный шар|Часы|Монстр|Гора|Деревня",
    "tr-TR": "Kale|Ejderha|Ağaç ev|Korsan gemisi|Robot|Uzay istasyonu|Tren|Şelale|Yel değirmeni|Deniz feneri|Çiftlik|Uçak|Köprü|Volkan|Kardan adam|Roket|Bahçe|Denizaltı|Kedi|Köpek|Kütüphane|Tapınak|Kule|Ada|Hisar|Sıcak hava balonu|Saat|Canavar|Dağ|Köy",
}

def integer(n):
    return struct.pack(">I", n)

def utf(s):
    b = s.encode("utf-8")
    return struct.pack(">H", len(b)) + b

def build():
    count = 30
    chunks = {}
    for language, values in sorted(THEMES.items()):
        words = values.split("|")
        if len(words) != count:
            raise ValueError(language)
        chunks[language] = integer(2) + b"\x01" + utf(language) + integer(count) + b"".join(map(utf, words))
    return (integer(2) + integer(len(chunks)) + b"\x01" + integer(count)
            + b"".join(integer(i) for i in range(1, count+1))
            + b"".join(utf(k) + integer(len(v)) for k, v in chunks.items())
            + b"".join(chunks.values()))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, default=Path("Common/res/TitleUpdate/GameRules/gameModeLanguages.loc"))
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(build())
    print(args.output)
