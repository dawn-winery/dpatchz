# dpatchz

Rewrite of the directory patcher of [hdiffpatch](https://github.com/sisong/HDiffPatch) to be compatible with a certain anime company's diffs.

## Usage
```
dpatchz [-v] [-c cache_size] diff_file old_path new_path
```
After patching is complete `new_path` will have the new patched files. 

Note that files that have not been changed won't be in `new_path`.
