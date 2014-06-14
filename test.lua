function test_func(a,b,c)
	--print(phasor_version)
	t, i5, i6, n = test('12', 1, 2, "65")
	return 1,2,3,{4,5,6}
end

function bench_func(str, n)
	--print(str )
end

function test_func1(t, n)
	for k,v in pairs(t) do
		print(k .. " " .. v)
	end
	if (n == nil) then
		print("nil")
	end
end