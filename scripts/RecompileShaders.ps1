param (
    [string] $Root = "$PSScriptRoot/../.build",
    [string] $Compiler = "$env:VULKAN_SDK/Bin/glslc.exe"  
)

# Script that recompiles shaders in the shader cache using the glslc shipped with VulkanSDK
# It searches recursively for shader files, expands the includes and outputs the source file to an .exp.glsl file
# Finally compiles the shader into a spirv with embedded debug information

function ExpandIncludes {
    param (
        [string] $Path
    )
    
    $item = Get-Item $Path
    $includeDir = "$($item.Directory)/../vulkan/shaders"
    $outFile = "$($item.Directory)/$($item.BaseName).exp.glsl"

    $fileContent = (Get-Content $Path)

    while ($true) {
        $hasIncludes = $false

        $newFileContent = New-Object -TypeName "System.Text.StringBuilder";

        $fileContent -split [Environment]::NewLine | ForEach-Object {
            $m = $_ -match "#include <(.*)>$"

            if ($m) {
                $hasIncludes = $true
                $includeFullPath = "$includeDir/$($Matches[1]).glsl"
                Get-Content $includeFullPath | ForEach-Object { $newFileContent.AppendLine($_) | Out-Null }
            }
            else {
                $newFileContent.AppendLine($_) | Out-Null
            }
        }

        $fileContent = $newFileContent

        if (!$hasIncludes) {
            break
        }
    }

    Set-Content -Path $outFile -Value $fileContent
}

function CompileShader {
    param (
        [string] $Path,
        [string] $ShaderStage
    )

    $item = Get-Item $Path
    $outFile = "$($item.Directory)/$($item.BaseName).spirv"
    
    $debugFlags = "-g"
    $optimizationFlags = "-O0"

    & $Compiler --target-env=vulkan1.3 --target-spv=spv1.5 $optimizationFlags $debugFlags "-fshader-stage=$ShaderStage" -o $outFile $Path
}

Get-ChildItem -Path $Root -Recurse -ErrorAction SilentlyContinue -Force | ForEach-Object {
    $stage = $null

    switch ($_.Extension) {
        ".vert" { 
            $stage = "vertex"
        }

        ".frag" { 
            $stage = "fragment"
        }

        ".comp" { 
            $stage = "compute"
        }
    }

    if ($stage) {
        $expandedGlsl = "$($_.Directory)/$($_.BaseName).exp.glsl"
        $expandedSpirv = "$($_.Directory)/$($_.BaseName).exp.spirv"
        $originalSpirv = "$($_.Directory)/$($_.BaseName).spirv"

        ExpandIncludes -Path $_.FullName
        CompileShader -Path $expandedGlsl -ShaderStage $stage

        Remove-Item $originalSpirv -ErrorAction SilentlyContinue
        Rename-Item $expandedSpirv $originalSpirv
    }
}