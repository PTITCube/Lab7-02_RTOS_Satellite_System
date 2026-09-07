import os

Import("env")

env_file = ".env"

if os.path.isfile(env_file):
    print("Reading .env file...")
    with open(env_file, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            
            if "=" in line:
                key, value = line.split("=", 1)
                key = key.strip()
                value = value.strip()
                
                is_string = False
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                    is_string = True
                elif value.startswith("'") and value.endswith("'"):
                    value = value[1:-1]
                    is_string = True
                
                if is_string:
                    env.Append(CPPDEFINES=[(key, env.StringifyMacro(value))])
                else:
                    env.Append(CPPDEFINES=[(key, value)])
else:
    print(f"Warning: {env_file} not found. Ensure required credentials are set.")
