import sys
import os

def compile_forth(input_file, output_file):
    with open(input_file, 'r') as f:
        lines = f.readlines()

    compiled_tokens = []
    for line in lines:
        # Strip comments (anything after '\')
        code = line.split('\\')[0].strip()
        if code:
            # Force uppercase and split by spaces
            tokens = code.upper().split()
            compiled_tokens.extend(tokens)

    # Join with a single space to save precious EEPROM memory
    binary_payload = " ".join(compiled_tokens).encode('ascii')

    # The 24AA01 EEPROM only holds 128 Bytes!
    if len(binary_payload) > 128:
        print(f"WARNING: Payload is {len(binary_payload)} bytes. It will NOT fit on a 24AA01 EEPROM (Max 128 bytes).")
    else:
        print(f"SUCCESS: Payload compiled to {len(binary_payload)} bytes.")

    with open(output_file, 'wb') as f:
        f.write(binary_payload)
        f.write(b'\0') # Null terminator for safety

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python compile.py <input.forth> <output.bin>")
    else:
        compile_forth(sys.argv[1], sys.argv[2])