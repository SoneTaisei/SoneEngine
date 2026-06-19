$files = Get-ChildItem -Path .\Engine, .\Project -Recurse -Include *.cpp,*.h
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
foreach ($file in $files) {
    $content = [System.IO.File]::ReadAllText($file.FullName)
    $original = $content
    $content = $content -replace '"Sprite/', '"resources/Sprite/'
    $content = $content -replace '"Object/', '"resources/Object/'
    $content = $content -replace '"Model/', '"resources/Model/'
    $content = $content -replace '"shaders/', '"resources/shaders/'
    $content = $content -replace 'L"shaders/', 'L"resources/shaders/'
    $content = $content -replace '"json/', '"resources/json/'
    $content = $content -replace 'resources/resources/', 'resources/'
    if ($content -cne $original) {
        [System.IO.File]::WriteAllText($file.FullName, $content, $utf8NoBom)
        Write-Host "Modified $($file.FullName)"
    }
}
