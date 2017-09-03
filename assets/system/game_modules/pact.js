/**
 *  Sphere Runtime for Sphere games
 *  Copyright (c) 2015-2017, Fat Cerberus
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of miniSphere nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
**/

'use strict';

class Pact
{
	constructor()
	{
		this.m_handlers = [];
	}

	get [Symbol.toStringTag]()
	{
		return 'Pact';
	}

	makePromise()
	{
		var handler;
		var promise = new Promise((resolve, reject) => {
			handler = { resolve: resolve, reject: reject };
		})
		handler.that = promise;
		this.m_handlers.push(handler);
		return promise;
	}

	resolve(promise, value)
	{
		let handler = this.m_getHandler(promise);
		handler.resolve(value);
	}

	reject(promise, reason)
	{
		let handler = this.m_getHandler(promise);
		handler.reject(reason);
	}

	welsh(reason)
	{
		for (let i = this.m_handlers.length - 1; i >= 0; --i)
			this.m_handlers[i].reject(reason);
	}

	m_getHandler(promise)
	{
		if (!(promise instanceof Promise))
			throw new TypeError(`'${String(promise)}' is not a promise`);
		for (var i = this.m_handlers.length - 1; i >= 0; --i) {
			if (this.m_handlers[i].that == promise)
				return this.m_handlers[i];
		}
		throw new TypeError("unrecognized promise");
	}
}

exports = module.exports = Pact;
Object.assign(exports, {
	__esModule: true,
	default:   Pact,
	Pact:       Pact,
});
