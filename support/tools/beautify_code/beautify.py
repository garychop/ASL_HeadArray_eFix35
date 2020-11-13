import subprocess
from os import mkdir, path, unlink, listdir, getcwd


#
# Deletes all files in a folder, preserving the folder itself.
#
def DeleteFolderContents(directory):
    for the_file in listdir(directory):
        file_path = path.join(directory, the_file)
        try:
            if path.isfile(file_path):
                unlink(file_path)
        except Exception as e:
            print(e)
# End of DeleteFolderContents

if __name__ == "__main__":
	'''
	subprocess.call('uncrustify -c ./uncrustify.cfg -l C -f ../os_assert.c -o ../beautify_out/os_assert.c', shell=True)
	quit()
	try:
		call_str = "uncrustify -c uncrustify.cfg -l C -f {0}/os_assert.c -o {0}/beautify_out/os_assert.c".format(base_dir).replace('/c/', )
		print(call_str)
		# subprocess.call("uncrustify -c uncrustify.cfg -l C -f ../os_assert.c -o ../beautify_out/os_assert.c")
	except Exception as e:
		print(e)

	quit()
	'''

	output_dir = "../beautify_out"

	try:
		mkdir(output_dir)
	except:
		DeleteFolderContents(output_dir)
    
	for the_file in listdir('../'):
		file_path = path.join('../', the_file)
		try:
			if path.isfile(file_path):
				call_str = "uncrustify -c uncrustify.cfg -l C -f ../{0} -o {1}/{0}".format(the_file, output_dir)
				subprocess.call(call_str, shell=True)
		except Exception as e:
			print(e)