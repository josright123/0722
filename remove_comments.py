import sys
import re

def remove_comments(text):
    # Pattern to match strings, block comments, and line comments
    pattern = r'(\".*?\"|\'.*?\')|(/\*.*?\*/|//[^\r\n]*$)'
    regex = re.compile(pattern, re.MULTILINE | re.DOTALL)
    
    def replacer(match):
        if match.group(2) is not None:
            return ""
        else:
            return match.group(1)
            
    return regex.sub(replacer, text)

def main():
    file_path = r"d:\gemini3\0722\NUC9701 - JJ0.c"
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()
    except UnicodeDecodeError:
        with open(file_path, "r", encoding="mbcs") as f: # fallback for windows
            content = f.read()
            
    new_content = remove_comments(content)
    
    lines = new_content.split('\n')
    clean_lines = [line for line in lines if line.strip() != ""]
    new_content = "\n".join(clean_lines) + "\n"
    
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(new_content)

if __name__ == "__main__":
    main()
