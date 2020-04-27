-- instructions.lua

io.write("Hello world, from ", _VERSION, "!\n")
io.write("Generating source for Game Boy CPU instructions\n")

function trimline(line)
	return line
end

for line in io.lines("src/instructions") do
	line = trimline(line)
	if line == "" then
		continue
	end
	io.write(line, "\n")
end
