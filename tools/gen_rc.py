import os
import sys
import re
import shutil
import subprocess

def main():
    if len(sys.argv) != 5:
        print(f"用法: {sys.argv[0]} <generated_dir> <packer_exe> <version_dll> <solution_dir>")
        sys.exit(1)

    generated_dir = os.path.abspath(sys.argv[1])
    packer_exe    = os.path.abspath(sys.argv[2])
    version_dll   = os.path.abspath(sys.argv[3])
    solution_dir  = os.path.abspath(sys.argv[4])

    for label, p in [("Packer", packer_exe), ("version.dll", version_dll), ("SolutionDir", solution_dir)]:
        if not os.path.exists(p):
            print(f"[错误] {label} 不存在: {p}")
            sys.exit(1)

    os.makedirs(generated_dir, exist_ok=True)

    # ---------- 1. 复制 patch 文件夹到 generate ----------
    src_patch = os.path.join(solution_dir, "patch")
    if not os.path.isdir(src_patch):
        print(f"[错误] patch 文件夹不存在: {src_patch}")
        sys.exit(1)

    dst_patch = os.path.join(generated_dir, "patch")
    if os.path.exists(dst_patch):
        shutil.rmtree(dst_patch, ignore_errors=True)
    shutil.copytree(src_patch, dst_patch)

    # ---------- 2. 复制传入的 packer_exe 到 generate 并运行 ----------
    dst_packer = os.path.join(generated_dir, "PatchResourcePacker.exe")
    shutil.copy2(packer_exe, dst_packer)

    try:
        subprocess.check_call([dst_packer], cwd=generated_dir)
    except subprocess.CalledProcessError as e:
        print(f"[错误] packer 运行失败，返回码 {e.returncode}")
        sys.exit(1)

    seen_chs = os.path.join(generated_dir, "SEEN_CHS.TXT")
    if not os.path.isfile(seen_chs):
        print(f"[错误] packer 未生成 SEEN_CHS.TXT: {seen_chs}")
        sys.exit(1)

    # ---------- 3. 创建 res 文件夹并复制所需文件 ----------
    res_dir = os.path.join(generated_dir, "res")
    if os.path.exists(res_dir):
        shutil.rmtree(res_dir, ignore_errors=True)
    os.makedirs(res_dir, exist_ok=True)

    # 3.1 SEEN_CHS.TXT
    shutil.copy2(seen_chs, os.path.join(res_dir, "SEEN_CHS.TXT"))

    # 3.2 传入的 version.dll
    shutil.copy2(version_dll, os.path.join(res_dir, "version.dll"))

    # 3.3 solution_dir\gch 文件夹
    src_gch = os.path.join(solution_dir, "gch")
    if os.path.isdir(src_gch):
        shutil.copytree(src_gch, os.path.join(res_dir, "gch"))
    else:
        print(f"[警告] gch 文件夹不存在，跳过: {src_gch}")

    # 3.4 solution_dir\NOTICE.md
    src_notice = os.path.join(solution_dir, "NOTICE.md")
    if os.path.isfile(src_notice):
        shutil.copy2(src_notice, os.path.join(res_dir, "NOTICE.md"))
    else:
        print(f"[警告] NOTICE.md 不存在，跳过: {src_notice}")

    # ---------- 4. 扫描 res，生成 resource.h / resource.rc ----------
    files = []
    res_id = 1000
    for root, dirs, fs in os.walk(res_dir):
        dirs.sort()
        for name in sorted(fs):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, res_dir).replace("\\", "/")
            files.append((res_id, rel, full))
            res_id += 1

    def macro_of(rel: str) -> str:
        return "IDR_" + re.sub(r'[^A-Za-z0-9]', '_', rel).upper()

    header_path = os.path.join(generated_dir, "resource.h")
    with open(header_path, "w", encoding="utf-8", newline="\n") as h:
        h.write("#pragma once\n")
        h.write("#include <array>\n")
        h.write("#include <utility>\n\n")
        h.write(f"// 自动生成，请勿手改。资源数: {len(files)}\n\n")
        for rid, rel, _ in files:
            h.write(f"#define {macro_of(rel)} {rid}\n")
        h.write(f"\n#define IDR_FILE_COUNT {len(files)}\n\n")
        h.write("constexpr std::array<std::pair<const wchar_t*, unsigned>, "
                f"{len(files)}> PATCH_FILES = {{{{\n")
        for rid, rel, _ in files:
            wrel = rel.replace("/", "\\\\")
            h.write(f'    {{ L"{wrel}", {rid} }},\n')
        h.write("}};\n")

    rc_path = os.path.join(generated_dir, "resource.rc")
    with open(rc_path, "w", encoding="utf-8", newline="\r\n") as rc:
        rc.write('#include "resource.h"\n')
        rc.write('#include <windows.h>\n\n')
        for rid, rel, full in files:
            rc_path_str = full.replace("\\", "/")
            rc.write(f'{macro_of(rel)} RCDATA "{rc_path_str}"\n')

    print(f"[OK] 生成 {len(files)} 项资源 -> {generated_dir}")

if __name__ == "__main__":
    main()