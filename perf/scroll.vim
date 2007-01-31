runtime! debian.vim

function Scroll(dir, windiv)
	let wh = winheight(0)
	let i = 1
	while i < wh / a:windiv
		let i = i + 1
		if a:dir == "d"
			normal j
		else
			normal k
		end
		" insert a character to force vim to update!
		normal I 
		redraw
		normal dl
	endwhile
endfunction

function AutoScroll(count)
	let loop = 0
	while loop < a:count
		let loop = loop + 1
		call Scroll("d", 1)
		call Scroll("u", 2)
		call Scroll("d", 2)
		call Scroll("u", 1)
		call Scroll("d", 2)
		call Scroll("u", 2)
	endwhile
	quit!
endfunction
