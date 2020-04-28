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
	-- special cases
	direct = {
		["(n)"] = "READ (0xFF00 | READ (pc ++))",
		["(nn)"] = "READ ((READ (pc ++) << 8) | READ (pc ++))",
		["n"] = "READ (pc ++)",
		["nn"] = "(READ (pc ++) << 8) | READ (pc ++))",
		["-/-"] = "",
	}

	function param(p)
		if direct[p] ~= nil then
			return direct[p]
		elseif string.match(p, "%(.+%)") then -- (rr)
			return "READ(" .. string.match(p, "%((.+)%)") .. ")"
		else -- r
			return p
		end
	end

	return string.gsub(p, "([^,]+)", param)
end

-- create the function name
function fname(i, p)
	if string.match(p, "-/-") then
		suffix = ""
	else
		suffix = string.gsub(p, "[,%(%)]", "_")
	end
	return string.format("%s__%s", i, suffix)
end

-- turn the information in one line to an instruction
function instr(line)
	-- parse the line for the different tokens
	local tokens = {}
	for w in string.gmatch(line, '%S+') do tokens[#tokens+1] = w end

	-- work each token accordingly
	local op = tokens[3]
	local it = string.lower(tokens[1])

	if string.len(op) > 2 then
		print(op, ": unsupported for now")
		return
	end

	local pm = tokens[2]
	local cc = tokens[4]

	print(
		string.format(
			"void %s() { %s(%s); }; operations[0x%s] = %s;",
			fname(it, pm),
			it,
			params(pm),
			op,
			fname(it, pm)
		)
	)

end

-- iterate over all lines in the file and create an instruction for each
for line in io.lines("src/instructions") do
	line = trimline(line)
	if line ~= "" then instr(line) end
end
