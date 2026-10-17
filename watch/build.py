import argparse
from pathlib import Path
import subprocess
import zipfile

from oracle.build import REFERENCE, ROOT as ORACLE, build as build_oracle


def build(output):
    build_oracle()
    instance = ORACLE / "instance"
    output.mkdir(parents=True, exist_ok=False)
    classes = output / "classes"
    classes.mkdir()
    java = str(instance / "jre/bin/java")
    dependencies = [instance / name for name in ("ModTheSpire.jar", "desktop-1.0.jar", "classes")]
    sources = sorted((Path(__file__).parent / "java").glob("*.java"))
    subprocess.run([java, "-jar", str(REFERENCE / "native-protocol-source/ecj-4.8.jar"),
                    "-1.8", "-proc:none", "-nowarn", "-classpath", ":".join(map(str, dependencies)),
                    "-d", str(classes), *map(str, sources)], check=True)
    subprocess.run([java, "-cp", ":".join(map(str, [classes, *dependencies])), "BuildWatch",
                    str(classes), *map(str, dependencies), str(classes)], check=True)
    with zipfile.ZipFile(instance / "mods/Oracle.jar") as original, zipfile.ZipFile(output / "Watch.jar", "w", zipfile.ZIP_DEFLATED) as jar:
        replacements = {str(path.relative_to(classes)): path for path in (classes / "oracle").glob("*.class")}
        for name in original.namelist():
            if name not in replacements:
                jar.writestr(name, original.read(name))
        for name, path in replacements.items():
            jar.write(path, name)
    print(output / "Watch.jar", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build the watch adapter with the unchanged oracle game patches")
    parser.add_argument("--output", required=True, type=Path)
    build(parser.parse_args().output.resolve())
