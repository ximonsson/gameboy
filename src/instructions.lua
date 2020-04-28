-- instructions.lua

io.write("Hello world, from ", _VERSION, "!\n")
io.write("Generating source for Game Boy CPU instructions\n")

-- trim the line from comments and whitespace
function trimline(line)
	line = string.gsub(line, "#.*", "") -- remove comments
	line = line:match'^()%s*$' and '' or line:match'^%s*(.*%S)'
	return line
end

-- generate code for the parsed parameters
function params(p)

end

-- turn the information in one line to an instruction
function instr(line)
	-- parse the line for the different tokens
	local tokens = {}
	for w in string.gmatch(line, '%S+') do tokens[#tokens+1] = w end

	local instr_ = tokens[1]
	local params = tokens[2]
	local opcode = tokens[3]
	local cycle  = tokens[4]

	-- parse parameters


end

-- iterate over all lines in the file and create an instruction for each
for line in io.lines("src/instructions") do
	line = trimline(line)
	if line ~= "" then instr(line) end
end
