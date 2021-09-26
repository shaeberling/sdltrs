import os
import sys

def main(out_path):
  prefix_str = "#ifndef __TRS_XRAY_RESOURCES_H__\n#define __TRS_XRAY_RESOURCES_H__\n\n"
  postfix_str = "\n#endif  // __TRS_XRAY_RESOURCES_H__"
  html_str = escape_c_string(read_file("src/trs_xray.html"))
  ts_str = escape_c_string(read_file("dist/webpack/trs_xray.js"))
  mem_regions_str = escape_c_string(read_file("src/memory_regions.js"))
  css_str = escape_c_string(read_file("src/trs_xray.css"))

  jquery_str = escape_c_string(read_file("src/jquery.js"))

  norm_out_path = normalize_path(out_path)
  write_file(f'{norm_out_path}/trs_xray_resources.h',
               prefix_str,
               html_str,
               ts_str + jquery_str + mem_regions_str,
               css_str,
               postfix_str)

def write_file(filename, prefix_str, html_str, js_str, css_str, postfix_str):
  filename = normalize_path(filename)
  try:
    with open(filename, "w") as f:
      f.write(prefix_str)
      f.write(f'char* trs_xray_html = "{html_str}";\n')
      f.write(f'char* trs_xray_js = "{js_str}";\n')
      f.write(f'char* trs_xray_css = "{css_str}";\n')
      f.write(postfix_str)
  except:
    print(f"Generator: Cannot write to file '${filename}'.")
  print(f"Wrote to '${filename}'.")

def read_file(filename) -> str:
  filename = normalize_path(filename)
  try:
    with open(filename, "r") as f:
      contents = f.read()
    return contents
  except:
    print(f"Generator: Cannot read file {filename}")
    exit(1)
  return ""

def normalize_path(filename) -> str:
  path = os.path.dirname(os.path.realpath(__file__))
  return os.path.join(path, filename)

def escape_c_string(content) -> str:
  return content.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n\\\n').replace('%', '%%')


if __name__ == "__main__":
  if len(sys.argv) < 2:
    print("Argument needed: Output path", file=sys.stderr)
    exit(1)
  main(sys.argv[1])
