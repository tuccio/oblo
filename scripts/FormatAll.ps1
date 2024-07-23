param(
    [switch]
    $Cpp,
    [switch]
    $Shaders
)

function FormatFiles([string[]] $Include) {
    
    $targetFiles = @(
        Get-ChildItem -Path $PSScriptRoot/.. -Directory -Exclude 3rdparty, conan, testing, .* |
        Select-Object -ExpandProperty FullName |
        Get-ChildItem -Recurse -Include $Include |
        Select-Object -ExpandProperty FullName
    )

    $formatParams = @(
        '-i'          # In-place
        '-style=file' # Search for a .clang-format file in the parent directory of the source file.
        '-verbose'
    )
    
    clang-format $formatParams $targetFiles
}

$anySwitch = $Cpp -or $Shaders

if (!$anySwitch -or $Cpp) {
    & FormatFiles -Include *.cpp, *.hpp, *.inl
}

if (!$anySwitch -or $Shaders) {
    & FormatFiles -Include *.glsl, *.vert, *.frag, *.mesh, *.comp, *.rgen, *.rint, *.rmiss, *.rahit, *.rchit, *.rcall
}
