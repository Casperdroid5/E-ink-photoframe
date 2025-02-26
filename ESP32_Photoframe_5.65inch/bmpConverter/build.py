from cx_Freeze import setup, Executable

setup(
    name="BMP Converter",
    version="1.0",
    description="Convert Images to BMP",
    executables=[Executable("converter.py", base="Win32GUI")],
    options={
        "build_exe": {
            "include_files": ["imageHandler.py"],  # Add any additional files or directories you want to include
            "optimize": 2,  # Optimize the generated bytecode
        }
    }
)