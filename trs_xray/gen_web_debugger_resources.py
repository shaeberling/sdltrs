import os
import sys

def main(out_path):
  html_str = escape_c_string(read_file("src/web_debugger.html"))
  ts_str = escape_c_string(read_file("dist/web_debugger.js"))
  mem_regions_str = escape_c_string(read_file("src/memory_regions.js"))
  css_str = escape_c_string(read_file("src/web_debugger.css"))

  jquery_str = escape_c_string(read_file("src/jquery.js"))

  norm_out_path = normalize_path(out_path)
  write_file(f'{norm_out_path}/web_debugger_resources.h',
               html_str, ts_str + jquery_str + mem_regions_str, css_str)

def write_file(filename, html_str, js_str, css_str):
  filename = normalize_path(filename)
  try:
    with open(filename, "w") as f:
      f.write(f'char* web_debugger_html = "{html_str}";\n')
      f.write(f'char* web_debugger_js = "{js_str}";\n')
      f.write(f'char* web_debugger_css = "{css_str}";\n')
  except:
    print(f"Cannot write to file '${filename}'.")
  print(f"Wrote to '${filename}'.")

def read_file(filename) -> str:
  filename = normalize_path(filename)
  try:
    with open(filename, "r") as f:
      contents = f.read()
    return contents
  except:
    print(f"Cannot read file {filename}")
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
