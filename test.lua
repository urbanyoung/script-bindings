function test_func(a,b,c)
	--print(phasor_version)
	test('12', 1, 2, "65")
	return 1,2,3,{4,5,6}
end

function test_func1(t)
	for k,v in pairs(t) do
		print(k .. " " .. v)
	end
end