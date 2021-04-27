import os

def main():
  html_str = escape_c_string(read_file("web_debugger.html"))
  js_str = escape_c_string(read_file("web_debugger.js"))
  css_str = escape_c_string(read_file("web_debugger.css"))
  write_file("web_debugger_resources.h", html_str, js_str, css_str)

def write_file(filename, html_str, js_str, css_str):
  filename = normalize_path(filename)
  try:
    with open(filename, "w") as f:
      f.write(f'char* web_debugger_html = "{html_str}";\n')
      f.write(f'char* web_debugger_js = "{js_str}";\n')
      f.write(f'char* web_debugger_css = "{css_str}";\n')
  except:
    print(f"Cannot write to file '${filename}'.")

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
  return content.replace('"', '\\"').replace('\n', '\\\n')


if __name__ == "__main__":
    main()