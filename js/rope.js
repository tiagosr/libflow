var RopeNode = function(seq, data, children) {
	this.seq = seq;
	this.data = data;
	this.children = children || [];
};
RopeNode.prototype.find = function (part) {
	if (part.slice(0,this.seq.length)==this.seq) {
		var subpart = part.slice(this.seq.length);
		if(subpart == "") {
			return this.data;
		} else {
			for (var child in this.children) {
				var res = child.find(subpart);
				if (res != null) {
					return res;
				}
			}
		}
	}
	return null;
}
RopeNode.prototype.findAllBeginningWith = function (accum, part) {
	var res = [];
	if (part.indexOf(this.seq) == 0) {
		var newaccum = accum+this.seq;
		if (this.data) {
			res.push(newaccum);
		}
		var newpart = part.slice(this.seq.length);
		for (var child in this.children) {
			res.push(child.findAllBeginningWith(newaccum, newpart));
		}
	}
	return res;
}
var spliceStringDeltas = function (origstr, newstr) {
	if(origstr.charAt(0)<newstr.charAt(0)) {
		return [origstr, newstr];
	} else if (origstr.charAt(0)>newstr.charAt(0)) {
		return [newstr, origstr];
	} else {
		if (origstr=="") {
			if (origstr == newstr) {
				return [origstr];
			} else {
				return [origstr, '', newstr.splice(1)];
			}
		} else if (newstr == "") {
			return [origstr, ""];
		} else {
			var res = spliceStringDeltas(origstr.slice(1),newstr.slice(1));
			if (res.length == 2) {
				res.unshift(origstr.charAt(0));
			} else {
				res[0] = origstr.charAt(0)+res[0];
			}
			return res;			
		}
	}
}
RopeNode.prototype.add = function (data, part) {
	var deltas = spliceStringDeltas(this.seq, part);
	if (deltas[0].length > 0) {
		if (deltas[0] == this.seq) {
			for (var child in this.children) {
				child.add(data, part.splice(this.seq.length));
			}
			return this;
		} else {
			return new RopeNode(deltas[0],null,[new RopeNode(deltas[1],this.data,this.children),new RopeNode(deltas[2],data,[])]);
		}
	} else {
		/* bail out and return this object to keep the tree up */
		return this;
	}
}

var Rope = function() {
	this.children = [];
	this.find = function (name) {
		for (var child in this.children) {
			var res = child.find(subpart);
			if (res != null) {
				return res;
			}
		}
		return false;
	}
	this.findAllBeginningWith = function (name) {
		var res = [];
		for (var child in this.children) {
			res.push(child.findAllBeginningWith("",subpart));
		}
		return res;
	}
}

process.stdout.write("test different strings: "+String(spliceStringDeltas("oie","oio"))+"\n");
process.stdout.write("test equal strings: "+String(spliceStringDeltas("oie","oie"))+"\n");
process.stdout.write("test string and superset: "+String(spliceStringDeltas("oie","oieie"))+"\n");
process.stdout.write("test string and subset: "+String(spliceStringDeltas("oieie","oie"))+"\n");
